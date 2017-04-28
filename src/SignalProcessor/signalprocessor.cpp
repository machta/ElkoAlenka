#include "signalprocessor.h"

#include "../DataModel/opendatafile.h"
#include <AlenkaFile/datafile.h>
#include "../myapplication.h"
#include <AlenkaSignal/openclcontext.h>
#include <AlenkaSignal/filter.h>
#include <AlenkaSignal/filterprocessor.h>
#include <AlenkaSignal/montageprocessor.h>
#include "../options.h"
#include "../error.h"

#include <QFile>

#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <sstream>

using namespace std;
using namespace AlenkaFile;

namespace
{

const AbstractTrackTable* getTrackTable(OpenDataFile* file)
{
	return file->dataModel->montageTable()->trackTable(OpenDataFile::infoTable.getSelectedMontage());
}

void multiplySamples(vector<float>* samples)
{
	// Assume input is already sorted.
	vector<pair<double, double>> input = OpenDataFile::infoTable.getFrequencyMultipliers();
	if (input.empty())
		return;

	int inputSize = static_cast<int>(input.size());
	int samplesSize = static_cast<int>(samples->size());
	input.push_back(make_pair(samplesSize, input.back().second)); // End of vector guard.

	vector<float> multipliers(samplesSize, 1);

	for (int i = 0; i < inputSize; ++i)
	{
		double multi = input[i].second;
		int f = round(input[i].first);
		double nextF = input[i + 1].first;

		if (samplesSize < f || samplesSize < nextF)
			continue;

		for (; f < nextF; ++f)
			multipliers[f] = multi;
	}

	for (int i = 0; i < samplesSize; ++i)
		(*samples)[i] *= multipliers[i];
}

class FloatAllocator : public LRUCacheAllocator<float>
{
	const int size;
	int* destroyCounter;

public:
	FloatAllocator(int size) : size(size) {}

	virtual bool constructElement(float** ptr) override
	{
		*ptr = new float[size];
		return true;
	}
	virtual void destroyElement(float* ptr) override
	{
		delete[] ptr;
	}
};

} // namespace

SignalProcessor::SignalProcessor(unsigned int nBlock, unsigned int parallelQueues, int montageCopyCount, function<void ()> glSharing,
	OpenDataFile* file, AlenkaSignal::OpenCLContext* context, int extraSamplesFront, int extraSamplesBack)
	: nBlock(nBlock), parallelQueues(parallelQueues), montageCopyCount(montageCopyCount), glSharing(glSharing),
		file(file), context(context), extraSamplesFront(extraSamplesFront), extraSamplesBack(extraSamplesBack)
{
	fileChannels = file->file->getChannelCount();
	cl_int err;
	size_t size = (nBlock + 2)*fileChannels*sizeof(float);

	for (unsigned int i = 0; i < parallelQueues; ++i)
	{
		commandQueues.push_back(clCreateCommandQueue(context->getCLContext(), context->getCLDevice(), 0, &err));
		checkClErrorCode(err, "clCreateCommandQueue()");

		cl_mem_flags flags = CL_MEM_READ_WRITE;
		rawBuffers.push_back(clCreateBuffer(context->getCLContext(), flags, size, nullptr, &err));
		checkClErrorCode(err, "clCreateBuffer()");

#ifdef NDEBUG
		if (!PROGRAM_OPTIONS["cl11"].as<bool>())
			flags |= CL_MEM_HOST_NO_ACCESS;
#endif
		filterBuffers.push_back(clCreateBuffer(context->getCLContext(), flags, size, nullptr, &err));
		checkClErrorCode(err, "clCreateBuffer()");

		filterProcessors.push_back(new AlenkaSignal::FilterProcessor<float>(nBlock, fileChannels, context));
	}

	int blockFloats = nBlock*fileChannels;
	int64_t fileCacheMemory = PROGRAM_OPTIONS["fileCacheSize"].as<int>();
	fileCacheMemory *= 1000*1000/sizeof(float);
	int capacity = max(1, static_cast<int>(fileCacheMemory/blockFloats));

	logToFile("Creating File cache with " << capacity << " capacity and blocks of size " << blockFloats*sizeof(float) << ".");
	cache = new LRUCache<int, float>(capacity, new FloatAllocator(blockFloats));

	QFile headerFile(":/montageHeader.cl"); // TODO: Consolidate the 4 copies of this into one instance.
	headerFile.open(QIODevice::ReadOnly);
	header = headerFile.readAll().toStdString();

	updateFilter();
	setUpdateMontageFlag();
}

SignalProcessor::~SignalProcessor()
{
	cl_int err;

	deleteMontage();

	for (unsigned int i = 0; i < parallelQueues; ++i)
	{
		err = clReleaseCommandQueue(commandQueues[i]);
		checkClErrorCode(err, "clReleaseCommandQueue()");

		err = clReleaseMemObject(rawBuffers[i]);
		checkClErrorCode(err, "clReleaseMemObject()");

		err = clReleaseMemObject(filterBuffers[i]);
		checkClErrorCode(err, "clReleaseMemObject()");

		delete filterProcessors[i];
	}

	delete cache;
	delete montageProcessor;
	delete filter;
}

void SignalProcessor::updateFilter()
{
	using namespace std;

	if (!file)
		return;

	M = file->file->getSamplingFrequency() + 1;

	delete filter;
	filter = new AlenkaSignal::Filter<float>(M, file->file->getSamplingFrequency());

	filter->setLowpassOn(OpenDataFile::infoTable.getLowpassOn());
	filter->setLowpass(OpenDataFile::infoTable.getLowpassFrequency());

	filter->setHighpassOn(OpenDataFile::infoTable.getHighpassOn());
	filter->setHighpass(OpenDataFile::infoTable.getHighpassFrequency());

	filter->setNotchOn(OpenDataFile::infoTable.getNotchOn());
	filter->setNotch(PROGRAM_OPTIONS["notchFrequency"].as<double>());

	auto samples = filter->computeSamples();
	if (OpenDataFile::infoTable.getFrequencyMultipliersOn())
		multiplySamples(&samples);

	for (unsigned int i = 0; i < parallelQueues; ++i)
	{
		filterProcessors[i]->changeSampleFilter(M, samples);
		filterProcessors[i]->applyWindow(OpenDataFile::infoTable.getFilterWindow());
	}

	nDiscard = filterProcessors[0]->discardSamples();
	nDelay = filterProcessors[0]->delaySamples();
	nMontage = nBlock - nDiscard;
	nSamples = nMontage - (extraSamplesFront + extraSamplesBack);

	OpenDataFile::infoTable.setFilterCoefficients(filterProcessors[0]->getCoefficients());
}

void SignalProcessor::setUpdateMontageFlag()
{
	if (file)
	{
		trackCount = 0;

		if (0 < file->dataModel->montageTable()->rowCount())
		{
			for (int i = 0; i < getTrackTable(file)->rowCount(); ++i)
			{
				if (getTrackTable(file)->row(i).hidden == false)
					++trackCount;
			}
		}

		if (trackCount > 0)
			updateMontageFlag = true;
	}
}

void SignalProcessor::process(const vector<int>& indexVector, const vector<cl_mem>& outBuffers)
{
#ifndef NDEBUG
	assert(ready());
	assert(0 < indexVector.size());
	assert(static_cast<unsigned int>(indexVector.size()) <= parallelQueues);
	assert(indexVector.size() == outBuffers.size());

	for (unsigned int i = 0; i < outBuffers.size(); ++i)
		for (unsigned int j = 0; j < outBuffers.size(); ++j)
			assert(i == j || (outBuffers[i] != outBuffers[j] && indexVector[i] != indexVector[j]));
#endif

	if (updateMontageFlag)
	{
		updateMontageFlag = false;
		updateMontage();
	}

	cl_int err;
	const unsigned int iters = min(parallelQueues, static_cast<unsigned int>(indexVector.size()));

	for (unsigned int i = 0; i < iters; ++i)
	{
		// Load the signal data into the file cache.
		int index = indexVector[i], cacheIndex;

		float* fileBuffer = cache->getAny(set<int>{index}, &cacheIndex);
		assert (!fileBuffer || cacheIndex == index);

		if (!fileBuffer)
		{
			fileBuffer = cache->setOldest(index);
			logToFileAndConsole("Loading block " << index << " to File cache.");

			auto fromTo = blockIndexToSampleRange(index, nSamples);
			fromTo.first += - nDiscard + nDelay - extraSamplesFront;
			fromTo.second += nDelay + extraSamplesBack;
			assert(fromTo.second - fromTo.first + 1 == nBlock);

			file->file->readSignal(fileBuffer, fromTo.first, fromTo.second);
		}

		assert(fileBuffer);
		printBuffer("after_readSignal.txt", fileBuffer, nBlock*fileChannels);

		size_t origin[] = {0, 0, 0};
		size_t rowLen = nBlock*sizeof(float);
		size_t region[] = {rowLen, fileChannels, 1};

		err = clEnqueueWriteBufferRect(commandQueues[i], rawBuffers[i], CL_TRUE,
			origin, origin, region, rowLen + 2*sizeof(float), 0, 0, 0,
			fileBuffer, 0, nullptr, nullptr);
		checkClErrorCode(err, "clEnqueueWriteBufferRect()");

		if (!allpass())
		{
			// Enqueu the filter operation, and store the result in the second buffer.
			printBuffer("before_filter.txt", rawBuffers[i], commandQueues[i]);
			filterProcessors[i]->process(rawBuffers[i], filterBuffers[i], commandQueues[i]);
			printBuffer("after_filter.txt", filterBuffers[i], commandQueues[i]);
		}
	}

	// Synchronize with GL so that we can use the shared buffers.
	if (glSharing)
		glSharing(); // Could be replaced by a fence.

	// Enque the montage computation, and store the the result in the output buffer.
	for (unsigned int i = 0; i < iters; ++i)
	{
		if (glSharing)
		{
			err = clEnqueueAcquireGLObjects(commandQueues[i], 1, &outBuffers[i], 0, nullptr, nullptr);
			checkClErrorCode(err, "clEnqueueAcquireGLObjects()");
		}

		cl_mem buffer = filterBuffers[i];
		int offset = nDiscard;
		if (allpass())
		{
			buffer = rawBuffers[i];
			offset -= nDelay;
		}

		montageProcessor->process(montage, buffer, outBuffers[i], commandQueues[i], nMontage, offset);
		printBuffer("after_montage.txt", outBuffers[i], commandQueues[i]);
	}

	// Release the locked buffers and wait for all operations to finish.
	for (unsigned int i = 0; i < iters; ++i)
	{
		if (glSharing)
		{
			err = clEnqueueReleaseGLObjects(commandQueues[i], 1, &outBuffers[i], 0, nullptr, nullptr);
			checkClErrorCode(err, "clEnqueueReleaseGLObjects()");
		}

		err = clFinish(commandQueues[i]);
		checkClErrorCode(err, "clFinish()");
	}
}

vector<AlenkaSignal::Montage<float>*> SignalProcessor::makeMontage(const vector<string>& montageCode,
	AlenkaSignal::OpenCLContext* context, KernelCache* kernelCache, const string& header)
{
#ifndef NDEBUG
	using namespace chrono;
	auto start = high_resolution_clock::now(); // TODO: Remove this after the compilation time issue is solved, or perhaps log this info to a file.
	int needToCompile = 0;
#endif

	vector<AlenkaSignal::Montage<float>*> montage;

	for (unsigned int i = 0; i < montageCode.size(); i++)
	{
		AlenkaSignal::Montage<float>* m;
		QString code = QString::fromStdString(simplifyMontage(montageCode[i]));

		auto ptr = kernelCache ? kernelCache->find(code) : nullptr;

		if (ptr)
		{
			assert(0 < ptr->size());
			m = new AlenkaSignal::Montage<float>(ptr, context);
		}
		else
		{
#ifndef NDEBUG
			++needToCompile;
#endif
			m = new AlenkaSignal::Montage<float>(code.toStdString(), context, header);

			if (kernelCache)
			{
				auto binary = m->getBinary();
				if (binary->size() > 0)
					kernelCache->insert(code, binary);
			}
		}

		montage.push_back(m);
	}

#ifndef NDEBUG
	auto end = high_resolution_clock::now();
	nanoseconds time = end - start;
	string str = "Need to compile " + to_string(needToCompile) + " montages: " + to_string(static_cast<double>(time.count())/1000/1000) + " ms";
	if (needToCompile > 0)
	{
		logToFileAndConsole(str);
	}
	else
	{
		logToFileAndConsole(str)
		//logToFile(str);
	}
#endif

	return montage;
}

void SignalProcessor::updateMontage()
{
	assert(ready());

	delete montageProcessor;
	montageProcessor = new AlenkaSignal::MontageProcessor<float>(nBlock + 2, fileChannels, montageCopyCount);

	deleteMontage();
	vector<string> montageCode;

	for (int i = 0; i < getTrackTable(file)->rowCount(); i++)
	{
		Track t = getTrackTable(file)->row(i);
		if (!t.hidden)
			montageCode.push_back(t.code);
	}

	montage = makeMontage(montageCode, context, file->kernelCache, header);
}

void SignalProcessor::deleteMontage()
{
	for (auto e : montage)
		delete e;
	montage.clear();
}

bool SignalProcessor::allpass()
{
	return OpenDataFile::infoTable.getFrequencyMultipliersOn() == false && filter->isAllpass();
}

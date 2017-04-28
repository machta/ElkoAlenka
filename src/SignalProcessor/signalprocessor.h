#ifndef SIGNALPROCESSOR_H
#define SIGNALPROCESSOR_H

#include <AlenkaSignal/montage.h>
#include "lrucache.h"

#include <CL/cl_gl.h>

#include <QString>

#include <cinttypes>
#include <set>
#include <vector>
#include <functional>

namespace AlenkaSignal
{
class OpenCLContext;
template<class T>
class FilterProcessor;
template<class T>
class MontageProcessor;
template<class T>
class Filter;
}
class OpenDataFile;
class SignalBlock;
class KernelCache;

/**
 * @brief A class used for retrieving the processed signal blocks.
 *
 * This is the main class in the SignalProcessor package. As such it is the only
 * one directly accessed from the rest of the code.
 *
 * Before an object of this class is constructed or used an OpenGL context must be set current.
 * There is example usage outside of a method that guarantees this:
 * @code{.cpp}
makeCurrent();
signalProcessor->setUpdateMontageFlag();
doneCurrent();
 * @endcode
 *
 */
class SignalProcessor
{
	int trackCount = 0;
	bool updateMontageFlag = false;

	int nBlock, nMontage, nSamples, M, nDelay, nDiscard;
	unsigned int parallelQueues, montageCopyCount, fileChannels;

	std::vector<cl_command_queue> commandQueues;
	std::vector<cl_mem> rawBuffers;
	std::vector<cl_mem> filterBuffers;
	LRUCache<int, float>* cache;

	std::function<void ()> glSharing;
	OpenDataFile* file;
	AlenkaSignal::OpenCLContext* context;
	std::vector<AlenkaSignal::FilterProcessor<float>*> filterProcessors;
	AlenkaSignal::MontageProcessor<float>* montageProcessor = nullptr;
	std::vector<AlenkaSignal::Montage<float>*> montage;
	std::string header;
	int extraSamplesFront, extraSamplesBack;
	AlenkaSignal::Filter<float>* filter = nullptr;

public:
	SignalProcessor(unsigned int nBlock, unsigned int parallelQueues, int montageCopyCount,
		std::function<void ()> glSharing, OpenDataFile* file, AlenkaSignal::OpenCLContext* context,
		int extraSamplesFront = 0, int extraSamplesBack = 0);
	~SignalProcessor();

	/**
	 * @brief Returns the number of channels of the result signal block.
	 */
	int getTrackCount() const
	{
		return trackCount;
	}

	int montageLength() { return nMontage; }

	/**
	 * @brief Updates the currently used filter according to the values set in the InfoTable.
	 *
	 * The filter is updated immediately.
	 */
	void updateFilter();

	/**
	 * @brief The montage will be updated next time it is needed.
	 *
	 * The update is lazy -- consecutive calls will do nothing.
	 * The reason for this is that needles repeated compilation of the montage code
	 * caused significant slowdown.
	 */
	void setUpdateMontageFlag();

	/**
	 * @brief Returns any block from indexSet ready to be used for rendering.
	 * @param indexSet Requested block indexes.
	 * @return An object wrapping the block index (and other info) and the vertex arrays for accessing the data.
	 *
	 * In this method the synchronization between OpenGL and OpenCL is performed
	 * through glFinish() and clFinish().
	 *
	 * Montage is updated if needed.
	 *
	 * If onlineFilter is set in Options filtration is performed here also.
	 *
	 * When called ready() should be true.
	 */
	void process(const std::vector<int>& indexVector, const std::vector<cl_mem>& outBuffers);

	/**
	 * @brief Returns true if this object is ready for full operation.
	 */
	bool ready() const
	{
		return file && trackCount > 0;
	}

	static std::string simplifyMontage(const std::string& str)
	{
		QString qstr = AlenkaSignal::Montage<float>::stripComments(str).c_str();
		return qstr.simplified().toStdString();
	}
	static std::vector<AlenkaSignal::Montage<float>*> makeMontage(const std::vector<std::string>& montageCode, AlenkaSignal::OpenCLContext* context, KernelCache* kernelCache, const std::string& header);

	static std::pair<std::int64_t, std::int64_t> blockIndexToSampleRange(int index, unsigned int blockSize)
	{
		using namespace std;

		int64_t from = index*(blockSize - 1);
		int64_t	to = from + blockSize - 1;

		return make_pair(from, to);
	}

private:
	/**
	 * @brief This method actually (unlike setUpdateMontageFlag()) updates the MontageProcessor object.
	 */
	void updateMontage();
	void deleteMontage();
	bool allpass();
};

#endif // SIGNALPROCESSOR_H

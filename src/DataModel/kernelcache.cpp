#include "kernelcache.h"

#include "../error.h"
#include "../options.h"

#include <QDir>
#include <QCoreApplication>

#include <fstream>
#include <sstream>

using namespace std;

namespace
{

string cacheFilePath()
{
	int platform = PROGRAM_OPTIONS["clPlatform"].as<int>();
	int device = PROGRAM_OPTIONS["clDevice"].as<int>();

	string str = PROGRAM_OPTIONS["kernelCacheDir"].as<string>();
	if (str.empty())
		str = QCoreApplication::applicationDirPath().toStdString();

	return str + QDir::separator().toLatin1() + "kernel-cache-" + to_string(platform) + '-' + to_string(device) + ".txt";
}

} // namespace

KernelCache::KernelCache()
{
	// TODO: Perhaps switch to byte-size based capacity system.
	cache.setMaxCost(PROGRAM_OPTIONS["kernelCacheSize"].as<int>());

	// Load.
	string filePath = cacheFilePath();
	ifstream file(filePath, ios::binary);

	if (file.is_open())
	{
		int size;
		file >> size;

		for (int i = 0; i < size; ++i)
		{
			size_t codeSize;
			file >> codeSize;
			file.ignore(2, '\n');

			unique_ptr<char[]> code(new char[codeSize + 1]());
			file.read(code.get(), codeSize);

			size_t binarySize;
			file >> binarySize;
			file.ignore(2, '\n');

			auto binary = new vector<unsigned char>(binarySize);
			file.read(reinterpret_cast<char*>(binary->data()), binarySize);

			cache.insert(QString(code.get()), binary);
		}
	}
	else
	{
		logToFileAndConsole("Failed to open kernel cache at " << filePath << ".");
	}
}

KernelCache::~KernelCache()
{
	// Save.
	int size = cache.size();
	if (size <= 0)
		return;

	string filePath = cacheFilePath();
	ofstream file(filePath, ios::binary);

	if (file.is_open())
	{
		file << size << endl;
		const auto& keys = cache.keys();

		for (auto qCode : keys)
		{
			size_t codeSize = qCode.size();
			string code = qCode.toStdString();

			file << codeSize << endl;
			file.write(code.data(), codeSize) << endl;

			auto binary = find(qCode);
			size_t binarySize = binary->size();

			file << binarySize << endl;
			file.write(reinterpret_cast<const char*>(binary->data()), binarySize) << endl;
		}
	}
	else
	{
		logToFileAndConsole("Failed to save kernel cache to " << filePath << ".");
	}
}

void KernelCache::deleteCacheFile()
{
	QFile::remove(QString::fromStdString(cacheFilePath()));
}

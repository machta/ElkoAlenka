#include "myapplication.h"

#include "options.h"
#include "error.h"
#include <AlenkaSignal/openclcontext.h>
#include <clFFT.h>

#include <QSurfaceFormat>
#include <QMessageBox>
#include <QLoggingCategory>

#include <stdexcept>
#include <string>

using namespace std;

unique_ptr<AlenkaSignal::OpenCLContext> globalContext(nullptr);

MyApplication::MyApplication(int& argc, char** argv) : QApplication(argc, argv)
{
	// Disable the stupid ssl warning.
	QLoggingCategory::setFilterRules("qt.network.ssl=false");

	try
	{
		// Set up the global options object.
		options = new Options(argc, argv);
		PROGRAM_OPTIONS_POINTER = SET_PROGRAM_OPTIONS_POINTER = options;

		// Set up the log.
		const int maxLogFileNameLength = 1000;
		char logFileName[maxLogFileNameLength + 1];
		time_t now = time(nullptr);
		size_t len = strftime(logFileName, maxLogFileNameLength, PROGRAM_OPTIONS["log"].as<string>().c_str(), localtime(&now));
		logFileName[len] = 0;

		LOG_FILE.open(logFileName);
		checkErrorCode(LOG_FILE.good(), true, "Could not open log file '" + string(logFileName) + "' for writing.");
	}
	catch (exception& e)
	{
		QMessageBox::critical(nullptr, "Exception caught", QString::fromStdString(e.what()));
		mainExit(EXIT_FAILURE);
	}
	catch (...)
	{
		QMessageBox::critical(nullptr, "Exception caught", "Unknown exception caught.");
		mainExit(EXIT_FAILURE);
	}

	// Log the command line parameters and config file.
	{
		stringstream ss;
		ss << "Starting with command: ";

		for (int i = 0; i < argc; ++i)
		{
			if (i != 0)
				ss << " ";

			ss << argv[i];
		}

		logToFile(ss.str());
	}

	PROGRAM_OPTIONS.logConfigFile();

	// Initialize the global OpenCL context.
	globalContext.reset(new AlenkaSignal::OpenCLContext(PROGRAM_OPTIONS["clPlatform"].as<int>(), PROGRAM_OPTIONS["clDevice"].as<int>()));

	// Set up the clFFT library.
	clfftStatus errFFT;
	clfftSetupData setupData;

	errFFT = clfftInitSetupData(&setupData);
	checkClfftErrorCode(errFFT, "clfftInitSetupData()");

	errFFT = clfftSetup(&setupData);
	checkClfftErrorCode(errFFT, "clfftSetup()");

	// Set some OpenGL context details.
	QSurfaceFormat format = QSurfaceFormat::defaultFormat();

	format.setVersion(2, 0);
	format.setProfile(QSurfaceFormat::NoProfile);

#ifndef NDEBUG
	format.setOption(QSurfaceFormat::DebugContext);
#endif

	QSurfaceFormat::setDefaultFormat(format);

	// Process some of the command-line only options.
	stringstream ss;

	ss << globalContext->getPlatformInfo() << endl << endl;
	ss << globalContext->getDeviceInfo() << endl;

	logToFile(ss.str());

	if (PROGRAM_OPTIONS.isSet("help"))
	{
		cout << PROGRAM_OPTIONS.getDescription() << endl;
		mainExit();
	}
	else if (PROGRAM_OPTIONS.isSet("clInfo"))
	{
		cout << ss.str();
		mainExit();
	}

	// Set locale.
	QLocale locale(PROGRAM_OPTIONS["locale"].as<string>().c_str());
	QLocale::setDefault(locale);
}

MyApplication::~MyApplication()
{
	clfftStatus errFFT = clfftTeardown();
	checkClfftErrorCode(errFFT, "clfftTeardown()");

	delete options;
}

bool MyApplication::notify(QObject* receiver, QEvent* event)
{
	try
	{
		return QApplication::notify(receiver, event);
	}
	catch (exception& e)
	{
		logToFileAndConsole("Standard exception caught: " << e.what());
	}
	catch (...)
	{
		logToFileAndConsole("Unknown exception caught.");
	}

	mainExit(EXIT_FAILURE);
	return false;
}

void MyApplication::mainExit(int status)
{
	logToFile("Exiting with status " << status << ".");
	std::exit(status);
}

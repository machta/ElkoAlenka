#ifndef FILTERVISUALIZER_H
#define FILTERVISUALIZER_H

#include <QWidget>

#include <unsupported/Eigen/FFT>

#include <vector>

class OpenDataFile;
class QAction;
namespace QtCharts
{
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
}

class FilterVisualizer : public QWidget
{
	Q_OBJECT

public:
	explicit FilterVisualizer(QWidget* parent = nullptr);
	~FilterVisualizer()
	{
		delete fft;
	}

	void changeFile(OpenDataFile* file);

public slots:
	void setChannelToDisplay(int value)
	{
		if (channelToDisplay != value)
		{
			channelToDisplay = value;
			forceUpdateSpectrum();
		}
	}
	void setSecondsToDisplay(int value)
	{
		if (secondToDisplay != value)
		{
			secondToDisplay = value;
			forceUpdateSpectrum();
		}
	}
	void setFreezeSpectrum(bool value)
	{
		if (freezeSpectrum != value)
		{
			freezeSpectrum = value;
			updateSpectrum();
		}
	}
	const QAction* getResetAction() const { return resetAction; }
	const QAction* getZoomAction() const { return zoomAction; }

private:
	OpenDataFile* file = nullptr;
	QtCharts::QChartView* chartView;
	std::vector<QMetaObject::Connection> connections;
	Eigen::FFT<float>* fft;
	std::vector<float> buffer;
	int channelToDisplay = 0;
	int secondToDisplay = 2;
	bool freezeSpectrum = true;
	QtCharts::QChart* chart;
	QtCharts::QLineSeries* spectrumSeries;
	QtCharts::QLineSeries* responseSeries;
	QtCharts::QValueAxis* axisX;
	QtCharts::QValueAxis* axisSpectrum;
	QtCharts::QValueAxis* axisResponse;
	QAction* resetAction;
	QAction* zoomAction;

	void forceUpdateSpectrum()
	{
		bool tmp = freezeSpectrum;
		freezeSpectrum = false;
		updateSpectrum();
		freezeSpectrum = tmp;
	}
	void addSeries();
	void removeSeries();

private slots:
	void updateSpectrum();
	void updateResponse();
};

#endif // FILTERVISUALIZER_H

#ifndef SPIKEDETSETTINGSDIALOG_H
#define SPIKEDETSETTINGSDIALOG_H

#include <AlenkaSignal/spikedet.h>

#include <QDialog>

class QSpinBox;
class QDoubleSpinBox;

class SpikedetSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SpikedetSettingsDialog(AlenkaSignal::DETECTOR_SETTINGS* settings, double* eventDuration, QWidget* parent = 0);
	static void resetSettings(AlenkaSignal::DETECTOR_SETTINGS* settings, double* eventDuration);

private:
	AlenkaSignal::DETECTOR_SETTINGS* settings;
	double* eventDuration;

	QSpinBox* fl_box;
	QSpinBox* fh_box;
	QDoubleSpinBox* k1_box;
	QDoubleSpinBox* k2_box;
	QDoubleSpinBox* k3_box;
	QSpinBox* w_box;
	QDoubleSpinBox* n_box;
	QSpinBox* buf_box;
	QSpinBox* h_box;
	QDoubleSpinBox* dt_box;
	QDoubleSpinBox* pt_box;
	QSpinBox* dec_box;
	QDoubleSpinBox* sed_box;

	void setValues();
};

#endif // SPIKEDETSETTINGSDIALOG_H

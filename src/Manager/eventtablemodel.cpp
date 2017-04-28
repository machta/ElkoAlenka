#include "eventtablemodel.h"

#include <AlenkaFile/datafile.h>
#include "../DataModel/opendatafile.h"
#include "../signalfilebrowserwindow.h"
#include "../DataModel/vitnessdatamodel.h"
#include "../DataModel/undocommandfactory.h"

#include <QLocale>
#include <QComboBox>

using namespace std;
using namespace AlenkaFile;

namespace
{

const AbstractTrackTable* currentTrackTable(OpenDataFile* file)
{
	return file->dataModel->montageTable()->trackTable(OpenDataFile::infoTable.getSelectedMontage());
}

const AbstractEventTable* currentEventTable(OpenDataFile* file)
{
	return file->dataModel->montageTable()->eventTable(OpenDataFile::infoTable.getSelectedMontage());
}

class Label : public TableColumn
{
public:
	Label(OpenDataFile* file) : TableColumn("Label", file) {}

	virtual QVariant data(int row, int role) const override
	{
		if (role == Qt::DisplayRole || role == Qt::EditRole)
			return QString::fromStdString(currentEventTable(file)->row(row).label);

		return QVariant();
	}

	virtual bool setData(int row, const QVariant& value, int role) override
	{
		if (role == Qt::EditRole)
		{
			Event e = currentEventTable(file)->row(row);
			e.label = value.toString().toStdString();
			file->undoFactory->changeEvent(OpenDataFile::infoTable.getSelectedMontage(), row, e, "change Label");
			return true;
		}

		return false;
	}
};

class Type : public TableColumn
{
public:
	Type(OpenDataFile* file) : TableColumn("Type", file) {}

	virtual QVariant data(int row, int role) const override
	{
		int type = currentEventTable(file)->row(row).type;

		if (role == Qt::DisplayRole)
			return QString::fromStdString(type < 0 ? NO_TYPE_STRING : file->dataModel->eventTypeTable()->row(type).name);
		else if (role == Qt::EditRole)
			return type;
		else if (role == Qt::DecorationRole && 0 <= type)
			return DataModel::array2color<QColor>(file->dataModel->eventTypeTable()->row(type).color);

		return QVariant();
	}

	virtual bool setData(int row, const QVariant& value, int role) override
	{
		if (role == Qt::EditRole)
		{
			Event e = currentEventTable(file)->row(row);
			e.type = value.toInt();
			file->undoFactory->changeEvent(OpenDataFile::infoTable.getSelectedMontage(), row, e, "change Type");
			return true;
		}

		return false;
	}

	virtual bool createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index, QWidget** widget) const override
	{
		(void)option; (void)index;

		QComboBox* combo = new QComboBox(parent);

		combo->addItem(NO_TYPE_STRING.c_str());
		for (int i = 0; i < file->dataModel->eventTypeTable()->rowCount(); ++i)
		{
			combo->addItem(QString::fromStdString(file->dataModel->eventTypeTable()->row(i).name));
		}

		*widget = combo;
		return true;
	}
	virtual bool setEditorData(QWidget* editor, const QModelIndex& index) const override
	{
		QComboBox* combo = reinterpret_cast<QComboBox*>(editor);
		int i = index.data(Qt::EditRole).toInt();
		combo->setCurrentIndex(i + 1);
		return true;
	}
	virtual bool setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
	{
		QComboBox* combo = reinterpret_cast<QComboBox*>(editor);
		model->setData(index, combo->currentIndex() - 1);
		return true;
	}
};

class Position : public TableColumn
{
public:
	Position(OpenDataFile* file) : TableColumn("Position", file) {}

	virtual QVariant data(int row, int role) const override
	{
		int position = currentEventTable(file)->row(row).position;

		if (role == Qt::DisplayRole)
			return SignalFileBrowserWindow::sampleToDateTimeString(file->file, position);
		else if (role == Qt::EditRole)
			return position;

		return QVariant();
	}

	virtual bool setData(int row, const QVariant& value, int role) override
	{
		if (role == Qt::EditRole)
		{
			Event e = currentEventTable(file)->row(row);
			e.position = value.toDouble();
			file->undoFactory->changeEvent(OpenDataFile::infoTable.getSelectedMontage(), row, e, "change Position");
			return true;
		}

		return false;
	}
};

class Duration : public TableColumn
{
public:
	Duration(OpenDataFile* file) : TableColumn("Duration", file) {}

	virtual QVariant data(int row, int role) const override
	{
		int duration = currentEventTable(file)->row(row).duration;

		if (role == Qt::DisplayRole)
			return SignalFileBrowserWindow::sampleToDateTimeString(file->file, duration);
		else if (role == Qt::EditRole)
			return duration;

		return QVariant();
	}

	virtual bool setData(int row, const QVariant& value, int role) override
	{
		if (role == Qt::EditRole)
		{
			Event e = currentEventTable(file)->row(row);
			e.duration = value.toDouble();
			file->undoFactory->changeEvent(OpenDataFile::infoTable.getSelectedMontage(), row, e, "change Duration");
			return true;
		}

		return false;
	}
};

class Channel : public TableColumn
{
public:
	Channel(OpenDataFile* file) : TableColumn("Channel", file) {}

	virtual QVariant data(int row, int role) const override
	{
		int channel = currentEventTable(file)->row(row).channel;

		if (role == Qt::DisplayRole)
		{
			string str;
			if (channel < 0)
			{
				if (channel == -1)
					str = ALL_CHANNEL_STRING;
				else
					str = NO_CHANNEL_STRING;
			}
			else
			{
				str = currentTrackTable(file)->row(channel).label;
			}

			return QString::fromStdString(str);
		}
		else if (role == Qt::EditRole)
		{
			return channel;
		}
		else if (role == Qt::DecorationRole && 0 <= channel)
		{
			return DataModel::array2color<QColor>(currentTrackTable(file)->row(channel).color);
		}

		return QVariant();
	}

	virtual bool setData(int row, const QVariant& value, int role) override
	{
		if (role == Qt::EditRole)
		{
			Event e = currentEventTable(file)->row(row);
			e.channel = value.toInt();
			file->undoFactory->changeEvent(OpenDataFile::infoTable.getSelectedMontage(), row, e, "change Channel");
			return true;
		}

		return false;
	}

	virtual bool createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index, QWidget** widget) const override
	{
		(void)option; (void)index;

		QComboBox* combo = new QComboBox(parent);

		combo->addItem(NO_CHANNEL_STRING.c_str());
		combo->addItem(ALL_CHANNEL_STRING.c_str());

		for (int i = 0; i < currentTrackTable(file)->rowCount(); ++i)
			combo->addItem(QString::fromStdString(currentTrackTable(file)->row(i).label));

		*widget = combo;
		return true;
	}
	virtual bool setEditorData(QWidget* editor, const QModelIndex& index) const override
	{
		QComboBox* combo = reinterpret_cast<QComboBox*>(editor);
		int i = index.data(Qt::EditRole).toInt();
		combo->setCurrentIndex(i + 2);
		return true;
	}
	virtual bool setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
	{
		QComboBox* combo = reinterpret_cast<QComboBox*>(editor);
		model->setData(index, combo->currentIndex() - 2);
		return true;
	}
};

class Description: public TableColumn
{
public:
	Description(OpenDataFile* file) : TableColumn("Description", file) {}

	virtual QVariant data(int row, int role) const override
	{
		if (role == Qt::DisplayRole || role == Qt::EditRole)
			return QString::fromStdString(currentEventTable(file)->row(row).description);

		return QVariant();
	}

	virtual bool setData(int row, const QVariant& value, int role) override
	{
		if (role == Qt::EditRole)
		{
			Event e = currentEventTable(file)->row(row);
			e.description= value.toString().toStdString();
			file->undoFactory->changeEvent(OpenDataFile::infoTable.getSelectedMontage(), row, e, "change Description");
			return true;
		}

		return false;
	}
};

} // namespace

EventTableModel::EventTableModel(OpenDataFile* file, QObject* parent) : TableModel(file, parent)
{
	columns.push_back(new Label(file));
	columns.push_back(new Type(file));
	columns.push_back(new Position(file));
	columns.push_back(new Duration(file));
	columns.push_back(new Channel(file));
	columns.push_back(new Description(file));

	connect(&OpenDataFile::infoTable, SIGNAL(selectedMontageChanged(int)), this, SLOT(setSelectedMontage(int)));
	setSelectedMontage(OpenDataFile::infoTable.getSelectedMontage());

	connect(&OpenDataFile::infoTable, SIGNAL(timeModeChanged(InfoTable::TimeMode)), this, SLOT(beginEndReset()));
}

int EventTableModel::rowCount(const QModelIndex& parent) const
{
	(void)parent;

	if (0 < file->dataModel->montageTable()->rowCount())
		return currentEventTable(file)->rowCount();
	return 0;
}

void EventTableModel::removeRowsFromDataModel(int row, int count)
{
	file->undoFactory->beginMacro("remove EventTable rows");
	file->undoFactory->removeEvent(OpenDataFile::infoTable.getSelectedMontage(), row, count);
	file->undoFactory->endMacro();
}

void EventTableModel::setSelectedMontage(int i)
{
	beginResetModel();

	for (auto e : montageTableConnections)
		disconnect(e);
	montageTableConnections.clear();

	if (0 < file->dataModel->montageTable()->rowCount())
	{
		auto vitness = VitnessEventTable::vitness(file->dataModel->montageTable()->eventTable(i));

		auto c = connect(vitness, SIGNAL(valueChanged(int, int)), this, SLOT(emitDataChanged(int, int)));
		montageTableConnections.push_back(c);

		c = connect(vitness, SIGNAL(rowsInserted(int, int)), this, SLOT(insertDataModelRows(int, int)));
		montageTableConnections.push_back(c);

		c = connect(vitness, SIGNAL(rowsRemoved(int, int)), this, SLOT(removeDataModelRows(int, int)));
		montageTableConnections.push_back(c);
	}

	endResetModel();
}

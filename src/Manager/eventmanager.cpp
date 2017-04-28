#include "eventmanager.h"

#include "../DataModel/opendatafile.h"
#include <AlenkaFile/datafile.h>
#include "../canvas.h"
#include "../DataModel/undocommandfactory.h"

#include <QTableView>
#include <QPushButton>
#include <QAction>

#include <algorithm>

using namespace std;
using namespace AlenkaFile;

EventManager::EventManager(QWidget* parent) : Manager(parent)
{
	QAction* goToAction = new QAction("Go To", this);
	goToAction->setShortcut(QKeySequence("g"));
	goToAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(goToAction, SIGNAL(triggered()), this, SLOT(goToEvent()));
	tableView->addAction(goToAction);

	QPushButton* goToButton = new QPushButton("Go To", this);
	connect(goToButton, SIGNAL(clicked()), this, SLOT(goToEvent()));
	addButton(goToButton);
}

bool EventManager::insertRowBack()
{
	if (file && 0 < file->dataModel->montageTable()->rowCount())
	{
		int rc = file->dataModel->montageTable()->eventTable(OpenDataFile::infoTable.getSelectedMontage())->rowCount();
		file->undoFactory->insertEvent(OpenDataFile::infoTable.getSelectedMontage(), rc, 1, "add Event row");
		return true;
	}

	return false;
}

// TODO: Don't constrain goto to the dimensions of the slider, but rather make sure time line is always correctly positioned.
void EventManager::goToEvent()
{
	if (!file)
		return;

	auto currentIndex = tableView->selectionModel()->currentIndex();

	if (file && currentIndex.isValid())
	{
		InfoTable& it = OpenDataFile::infoTable;

		double ratio = static_cast<double>(file->file->getSamplesRecorded())/it.getVirtualWidth();
		int col = static_cast<int>(Event::Index::position);
		auto index = tableView->model()->index(currentIndex.row(), col);

		double position = tableView->model()->data(index, Qt::EditRole).toInt();
		position /= ratio;
		position -= canvas->width()*it.getPositionIndicator();

		int intPosition = position;
		intPosition = min(max(0, intPosition), it.getVirtualWidth() - canvas->width() - 1);

		it.setPosition(intPosition);
	}
}

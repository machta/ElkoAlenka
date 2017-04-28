#include "undocommandfactory.h"

#include <QUndoStack>
#include <QUndoCommand>

#include <cassert>

using namespace std;
using namespace AlenkaFile;

namespace
{

class ChangeEventType : public QUndoCommand
{
	DataModel* dataModel;
	int i;
	EventType before, after;

public:
	ChangeEventType(DataModel* dataModel, int i, const EventType& value, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), after(value)
	{
		before = dataModel->eventTypeTable()->row(i);
	}
	virtual void redo() override
	{
		dataModel->eventTypeTable()->row(i, after);
	}
	virtual void undo() override
	{
		dataModel->eventTypeTable()->row(i, before);
	}
};

class ChangeMontage : public QUndoCommand
{
	DataModel* dataModel;
	int i;
	Montage before, after;

public:
	ChangeMontage(DataModel* dataModel, int i, const Montage& value, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), after(value)
	{
		before = dataModel->montageTable()->row(i);
	}
	virtual void redo() override
	{
		dataModel->montageTable()->row(i, after);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->row(i, before);
	}
};

class ChangeEvent : public QUndoCommand
{
	DataModel* dataModel;
	int i, j;
	Event before, after;

public:
	ChangeEvent(DataModel* dataModel, int i, int j, const Event& value, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), j(j), after(value)
	{
		before = dataModel->montageTable()->eventTable(i)->row(j);
	}
	virtual void redo() override
	{
		dataModel->montageTable()->eventTable(i)->row(j, after);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->eventTable(i)->row(j, before);
	}
};

class ChangeTrack : public QUndoCommand
{
	DataModel* dataModel;
	int i, j;
	Track before, after;

public:
	ChangeTrack(DataModel* dataModel, int i, int j, const Track& value, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), j(j), after(value)
	{
		before = dataModel->montageTable()->trackTable(i)->row(j);
	}
	virtual void redo() override
	{
		dataModel->montageTable()->trackTable(i)->row(j, after);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->trackTable(i)->row(j, before);
	}
};

class InsertEventType : public QUndoCommand
{
	DataModel* dataModel;
	int i, c;

public:
	InsertEventType(DataModel* dataModel, int i, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), c(c) {}
	virtual void redo() override
	{
		dataModel->eventTypeTable()->insertRows(i, c);
	}
	virtual void undo() override
	{
		dataModel->eventTypeTable()->removeRows(i, c);
	}
};

class InsertMontage: public QUndoCommand
{
	DataModel* dataModel;
	int i, c;

public:
	InsertMontage(DataModel* dataModel, int i, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), c(c) {}
	virtual void redo() override
	{
		dataModel->montageTable()->insertRows(i, c);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->removeRows(i, c);
	}
};

class InsertEvent: public QUndoCommand
{
	DataModel* dataModel;
	int i, j, c;

public:
	InsertEvent(DataModel* dataModel, int i, int j, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), j(j), c(c) {}
	virtual void redo() override
	{
		dataModel->montageTable()->eventTable(i)->insertRows(j, c);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->eventTable(i)->removeRows(j, c);
	}
};

class InsertTrack: public QUndoCommand
{
	DataModel* dataModel;
	int i, j, c;

public:
	InsertTrack(DataModel* dataModel, int i, int j, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), j(j), c(c) {}
	virtual void redo() override
	{
		dataModel->montageTable()->trackTable(i)->insertRows(j, c);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->trackTable(i)->removeRows(j, c);
	}
};

class RemoveEventType : public QUndoCommand
{
	DataModel* dataModel;
	int i, c;
	vector<EventType> before;

public:
	RemoveEventType(DataModel* dataModel, int i, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), c(c)
	{
		for (int k = 0; k < c; ++k)
			before.push_back(dataModel->eventTypeTable()->row(i + k));
	}
	virtual void redo() override
	{
		dataModel->eventTypeTable()->removeRows(i, c);
	}
	virtual void undo() override
	{
		dataModel->eventTypeTable()->insertRows(i, c);

		for (int k = 0; k < c; ++k)
			dataModel->eventTypeTable()->row(i + k, before[k]);
	}
};

class RemoveMontage: public QUndoCommand
{
	DataModel* dataModel;
	int i, c;
	vector<Montage> before;
	vector<vector<Event>> eventsBefore;
	vector<vector<Track>> tracksBefore;

public:
	RemoveMontage(DataModel* dataModel, int i, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), c(c)
	{
		eventsBefore.resize(c);
		tracksBefore.resize(c);

		AbstractMontageTable* mt = dataModel->montageTable();

		for (int k = 0; k < c; ++k)
			before.push_back(mt->row(i + k));

		for (int k = 0; k < c; ++k)
		{
			AbstractEventTable* et = mt->eventTable(i + k);
			int count = et->rowCount();
			for (int l = 0; l < count; ++l)
				eventsBefore[k].push_back(et->row(l));

			AbstractTrackTable* tt = mt->trackTable(i + k);
			count = tt->rowCount();
			for (int l = 0; l < count; ++l)
				tracksBefore[k].push_back(tt->row(l));
		}

	}
	virtual void redo() override
	{
		dataModel->montageTable()->removeRows(i, c);
	}
	virtual void undo() override
	{
		AbstractMontageTable* mt = dataModel->montageTable();
		mt->insertRows(i, c);

		for (int k = 0; k < c; ++k)
			mt->row(i + k, before[k]);

		for (int k = 0; k < c; ++k)
		{
			AbstractEventTable* et = mt->eventTable(i + k);
			int count = static_cast<int>(eventsBefore[k].size());
			et->insertRows(0, count);
			for (int l = 0; l < count; ++l)
				et->row(l, eventsBefore[k][l]);

			AbstractTrackTable* tt = mt->trackTable(i + k);
			count = static_cast<int>(tracksBefore[k].size());
			tt->insertRows(0, count);
			for (int l = 0; l < count; ++l)
				tt->row(l, tracksBefore[k][l]);
		}
	}
};

class RemoveEvent: public QUndoCommand
{
	DataModel* dataModel;
	int i, j, c;
	vector<Event> before;

public:
	RemoveEvent(DataModel* dataModel, int i, int j, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), j(j), c(c)
	{
		for (int k = 0; k < c; ++k)
			before.push_back(dataModel->montageTable()->eventTable(i)->row(j + k));
	}
	virtual void redo() override
	{
		dataModel->montageTable()->eventTable(i)->removeRows(j, c);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->eventTable(i)->insertRows(j, c);

		for (int k = 0; k < c; ++k)
			dataModel->montageTable()->eventTable(i)->row(j + k, before[k]);
	}
};

class RemoveTrack: public QUndoCommand
{
	DataModel* dataModel;
	int i, j, c;
	vector<Track> before;

public:
	RemoveTrack(DataModel* dataModel, int i, int j, int c, const QString& text) :
		QUndoCommand(text), dataModel(dataModel), i(i), j(j), c(c)
	{
		for (int k = 0; k < c; ++k)
			before.push_back(dataModel->montageTable()->trackTable(i)->row(j + k));
	}
	virtual void redo() override
	{
		dataModel->montageTable()->trackTable(i)->removeRows(j, c);
	}
	virtual void undo() override
	{
		dataModel->montageTable()->trackTable(i)->insertRows(j, c);

		for (int k = 0; k < c; ++k)
			dataModel->montageTable()->trackTable(i)->row(j + k, before[k]);
	}
};

} // namespace

void UndoCommandFactory::push(QUndoCommand* cmd)
{
	undoStack->push(cmd);
}

void UndoCommandFactory::beginMacro(const QString& text)
{
	undoStack->beginMacro(text);
}

void UndoCommandFactory::endMacro()
{
	undoStack->endMacro();
}

void UndoCommandFactory::changeEventType(int i, const EventType& value, const QString& text) const
{
	undoStack->push(new ChangeEventType(dataModel, i, value, text));
}

void UndoCommandFactory::changeMontage(int i, const Montage& value, const QString& text) const
{
	undoStack->push(new ChangeMontage(dataModel, i, value, text));
}

void UndoCommandFactory::changeEvent(int i, int j, const Event& value, const QString& text) const
{
	undoStack->push(new ChangeEvent(dataModel, i, j, value, text));
}

void UndoCommandFactory::changeTrack(int i, int j, const Track& value, const QString& text) const
{
	undoStack->push(new ChangeTrack(dataModel, i, j, value, text));
}

void UndoCommandFactory::insertEventType(int i, int c, const QString& text) const
{
	undoStack->push(new InsertEventType(dataModel, i, c, text));
}

void UndoCommandFactory::insertMontage(int i, int c, const QString& text) const
{
	undoStack->push(new InsertMontage(dataModel, i, c, text));
}

void UndoCommandFactory::insertEvent(int i, int j, int c, const QString& text) const
{
	undoStack->push(new InsertEvent(dataModel, i, j, c, text));
}

void UndoCommandFactory::insertTrack(int i, int j, int c, const QString& text) const
{
	undoStack->push(new InsertTrack(dataModel, i, j, c, text));
}

void UndoCommandFactory::removeEventType(int i, int c, const QString& text) const
{
	undoStack->push(new RemoveEventType(dataModel, i, c, text));
}

void UndoCommandFactory::removeMontage(int i, int c, const QString& text) const
{
	undoStack->push(new RemoveMontage(dataModel, i, c, text));
}

void UndoCommandFactory::removeEvent(int i, int j, int c, const QString& text) const
{
	undoStack->push(new RemoveEvent(dataModel, i, j, c, text));
}

void UndoCommandFactory::removeTrack(int i, int j, int c, const QString& text) const
{
	undoStack->push(new RemoveTrack(dataModel, i, j, c, text));
}

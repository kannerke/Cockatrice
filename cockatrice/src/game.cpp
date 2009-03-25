#include <QGraphicsScene>
#include <QMenu>
#include <QMessageBox>
#include "serverplayer.h"
#include "game.h"
#include "servereventdata.h"
#include "client.h"
#include "tablezone.h"
#include "handzone.h"
#include "carddatabase.h"
#include "dlg_startgame.h"

Game::Game(CardDatabase *_db, Client *_client, QGraphicsScene *_scene, QMenu *_actionsMenu, QMenu *_cardMenu, int playerId, const QString &playerName)
	: QObject(), actionsMenu(_actionsMenu), cardMenu(_cardMenu), db(_db), client(_client), scene(_scene), started(false)
{
	QRectF sr = scene->sceneRect();
	localPlayer = addPlayer(playerId, playerName, QPointF(0, sr.y() + sr.height() / 2), true);

	connect(client, SIGNAL(gameEvent(ServerEventData *)), this, SLOT(gameEvent(ServerEventData *)));
	connect(client, SIGNAL(playerListReceived(QList<ServerPlayer *>)), this, SLOT(playerListReceived(QList<ServerPlayer *>)));

	aUntapAll = new QAction(tr("&Untap all permanents"), this);
	aUntapAll->setShortcut(tr("Ctrl+U"));
	connect(aUntapAll, SIGNAL(triggered()), this, SLOT(actUntapAll()));

	aDecLife = new QAction(tr("&Decrement life"), this);
	aDecLife->setShortcut(tr("F11"));
	connect(aDecLife, SIGNAL(triggered()), this, SLOT(actDecLife()));
	aIncLife = new QAction(tr("&Increment life"), this);
	aIncLife->setShortcut(tr("F12"));
	connect(aIncLife, SIGNAL(triggered()), this, SLOT(actIncLife()));
	aSetLife = new QAction(tr("&Set life"), this);
	aSetLife->setShortcut(tr("Ctrl+L"));
	connect(aSetLife, SIGNAL(triggered()), this, SLOT(actSetLife()));

	aShuffle = new QAction(tr("&Shuffle"), this);
	aShuffle->setShortcut(tr("Ctrl+S"));
	connect(aShuffle, SIGNAL(triggered()), this, SLOT(actShuffle()));
	aDraw = new QAction(tr("&Draw a card"), this);
	aDraw->setShortcut(tr("Ctrl+D"));
	connect(aDraw, SIGNAL(triggered()), this, SLOT(actDrawCard()));
	aDrawCards = new QAction(tr("D&raw cards..."), this);
	connect(aDrawCards, SIGNAL(triggered()), this, SLOT(actDrawCards()));
	aRollDice = new QAction(tr("R&oll dice..."), this);
	aRollDice->setShortcut(tr("Ctrl+I"));
	connect(aRollDice, SIGNAL(triggered()), this, SLOT(actRollDice()));

	aCreateToken = new QAction(tr("&Create token..."), this);
	aCreateToken->setShortcut(tr("Ctrl+T"));
	connect(aCreateToken, SIGNAL(triggered()), this, SLOT(actCreateToken()));

	actionsMenu->addAction(aUntapAll);
	actionsMenu->addSeparator();
	actionsMenu->addAction(aDecLife);
	actionsMenu->addAction(aIncLife);
	actionsMenu->addAction(aSetLife);
	actionsMenu->addSeparator();
	actionsMenu->addAction(aShuffle);
	actionsMenu->addAction(aDraw);
	actionsMenu->addAction(aDrawCards);
	actionsMenu->addAction(aRollDice);
	actionsMenu->addSeparator();
	actionsMenu->addAction(aCreateToken);

	aTap = new QAction(tr("&Tap"), this);
	connect(aTap, SIGNAL(triggered()), this, SLOT(actTap()));
	aUntap = new QAction(tr("&Untap"), this);
	connect(aUntap, SIGNAL(triggered()), this, SLOT(actUntap()));
	aAddCounter = new QAction(tr("&Add counter"), this);
	connect(aAddCounter, SIGNAL(triggered()), this, SLOT(actAddCounter()));
	aRemoveCounter = new QAction(tr("&Remove counter"), this);
	connect(aRemoveCounter, SIGNAL(triggered()), this, SLOT(actRemoveCounter()));
	aSetCounters = new QAction(tr("&Set counters..."), this);
	connect(aSetCounters, SIGNAL(triggered()), this, SLOT(actSetCounters()));
	aRearrange = new QAction(tr("&Rearrange"), this);
	connect(aRearrange, SIGNAL(triggered()), this, SLOT(actRearrange()));

	cardMenu->addAction(aTap);
	cardMenu->addAction(aUntap);
	cardMenu->addAction(aAddCounter);
	cardMenu->addAction(aRemoveCounter);
	cardMenu->addAction(aSetCounters);
	cardMenu->addSeparator();
	cardMenu->addAction(aRearrange);
	
	dlgStartGame = new DlgStartGame(db);
	connect(dlgStartGame, SIGNAL(newDeckLoaded(const QStringList &)), client, SLOT(submitDeck(const QStringList &)));
	connect(dlgStartGame, SIGNAL(finished(int)), this, SLOT(readyStart(int)));
}

Game::~Game()
{
	qDebug("Game destructor");
	for (int i = 0; i < players.size(); i++) {
		emit playerRemoved(players.at(i));
		delete players.at(i);
	}
}

Player *Game::addPlayer(int playerId, const QString &playerName, QPointF base, bool local)
{
	Player *newPlayer = new Player(playerName, playerId, base, local, db, client);

	const ZoneList *const z = newPlayer->getZones();
	for (int i = 0; i < z->size(); i++)
		scene->addItem(z->at(i));

	const CounterList *const c = newPlayer->getCounters();
	for (int i = 0; i < c->size(); i++)
		scene->addItem(c->at(i));

	connect(newPlayer, SIGNAL(hoverCard(QString)), this, SIGNAL(hoverCard(QString)));
	connect(newPlayer, SIGNAL(sigShowCardMenu(QPoint)), this, SLOT(showCardMenu(QPoint)));
	connect(newPlayer, SIGNAL(logMoveCard(QString, QString, QString, QString)), this, SIGNAL(logMoveCard(QString, QString, QString, QString)));
	connect(newPlayer, SIGNAL(logCreateToken(QString, QString)), this, SIGNAL(logCreateToken(QString, QString)));
	connect(newPlayer, SIGNAL(logSetCardCounters(QString, QString, int, int)), this, SIGNAL(logSetCardCounters(QString, QString, int, int)));
	connect(newPlayer, SIGNAL(logSetTapped(QString, QString, bool)), this, SIGNAL(logSetTapped(QString, QString, bool)));
	connect(newPlayer, SIGNAL(logSetCounter(QString, QString, int, int)), this, SIGNAL(logSetCounter(QString, QString, int, int)));

	players << newPlayer;
	emit playerAdded(newPlayer);

	return newPlayer;
}

void Game::playerListReceived(QList<ServerPlayer *> playerList)
{
	QListIterator<ServerPlayer *> i(playerList);
	QStringList nameList;
	while (i.hasNext()) {
		ServerPlayer *temp = i.next();
		nameList << temp->getName();
		int id = temp->getPlayerId();

		if (id != localPlayer->getId())
			addPlayer(id, temp->getName(), QPointF(0, 0), false);

		delete temp;
	}
	emit logPlayerListReceived(nameList);
	restartGameDialog();
}

void Game::readyStart(int foo)
{
	Q_UNUSED(foo);
	
	client->readyStart();
}

void Game::restartGameDialog()
{
	dlgStartGame->show();
}

void Game::gameEvent(ServerEventData *msg)
{
	if (!msg->getPublic())
		localPlayer->gameEvent(msg);
	else {
		Player *p = players.findPlayer(msg->getPlayerId());
		if (!p) {
			// XXX
		}

		switch(msg->getEventType()) {
		case eventSay:
			emit logSay(p->getName(), msg->getEventData()[0]);
			break;
		case eventJoin: {
			emit logJoin(msg->getPlayerName());
			addPlayer(msg->getPlayerId(), msg->getPlayerName(), QPointF(0, 0), false);
			break;
		}
		case eventLeave:
			emit logLeave(msg->getPlayerName());
			// XXX Spieler natürlich noch rauswerfen
			break;
		case eventReadyStart:
			if (started) {
				started = false;
				emit logReadyStart(p->getName());
				if (!p->getLocal())
					restartGameDialog();
			}
			break;
		case eventGameStart:
			started = true;
			emit logGameStart();
			break;
		case eventShuffle:
			emit logShuffle(p->getName());
			break;
		case eventRollDice: {
			QStringList data = msg->getEventData();
			int sides = data[0].toInt();
			int roll = data[1].toInt();
			emit logRollDice(p->getName(), sides, roll);
			break;
		}
		case eventSetActivePlayer:
			break;
		case eventSetActivePhase:
			break;

		case eventName:
		case eventCreateToken:
		case eventSetupZones:
		case eventSetCardAttr:
		case eventSetCounter:
		case eventDelCounter:
		case eventPlayerId: {
			p->gameEvent(msg);
			break;
		}
		case eventDumpZone: {
			QStringList data = msg->getEventData();
			emit logDumpZone(p->getName(), data[1], players.findPlayer(data[0].toInt())->getName(), data[2].toInt());
			break;
		}
		case eventMoveCard: {
			if (msg->getPlayerId() == localPlayer->getId())
				break;
			p->gameEvent(msg);
			break;
		}
		case eventDraw: {
			emit logDraw(p->getName(), msg->getEventData()[0].toInt());
			if (msg->getPlayerId() == localPlayer->getId())
				break;
			p->gameEvent(msg);
			break;
		}
		case eventInvalid:
			qDebug("Unhandled global event");
		}
	}
}

void Game::actUntapAll()
{
	CardList *const cards = localPlayer->getZones()->findZone("table")->getCards();
	for (int i = 0; i < cards->size(); i++)
		client->setCardAttr("table", cards->at(i)->getId(), "tapped", "false");
}

void Game::actIncLife()
{
	client->incCounter("life", 1);
}

void Game::actDecLife()
{
	client->incCounter("life", -1);
}

void Game::actSetLife()
{
	bool ok;
	int life = QInputDialog::getInteger(0, tr("Set life"), tr("New life total:"), localPlayer->getCounters()->findCounter("life")->getValue(), 0, 2000000000, 1, &ok);
	client->setCounter("life", life);
}

void Game::actShuffle()
{
	client->shuffle();
}

void Game::actRollDice()
{
	bool ok;
	int sides = QInputDialog::getInteger(0, tr("Roll dice"), tr("Number of sides:"), 20, 2, 1000, 1, &ok);
	if (ok)
		client->rollDice(sides);
}

void Game::actDrawCard()
{
	client->drawCards(1);
}

void Game::actDrawCards()
{
	int number = QInputDialog::getInteger(0, tr("Draw cards"), tr("Number:"));
	if (number)
		client->drawCards(number);
}

void Game::actCreateToken()
{
	QString cardname = QInputDialog::getText(0, tr("Create token"), tr("Name:"));
	if (!db->getCard(cardname))
		QMessageBox::critical(0, "Error", "No such card");
	else
		client->createToken("table", cardname, QString(), 0, 0);
}

void Game::showCardMenu(QPoint p)
{
	cardMenu->exec(p);
}

void Game::actTap()
{
	QListIterator<QGraphicsItem *> i(scene->selectedItems());
	while (i.hasNext()) {
		CardItem *temp = (CardItem *) i.next();
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(temp->parentItem())->getName(), temp->getId(), "tapped", "1");
	}
}

void Game::actUntap()
{
	QListIterator<QGraphicsItem *> i(scene->selectedItems());
	while (i.hasNext()) {
		CardItem *temp = (CardItem *) i.next();
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(temp->parentItem())->getName(), temp->getId(), "tapped", "0");
	}
}

void Game::actAddCounter()
{
	QListIterator<QGraphicsItem *> i(scene->selectedItems());
	while (i.hasNext()) {
		CardItem *temp = (CardItem *) i.next();
		if (temp->getCounters() < MAX_COUNTERS_ON_CARD)
			client->setCardAttr(qgraphicsitem_cast<CardZone *>(temp->parentItem())->getName(), temp->getId(), "counters", QString::number(temp->getCounters() + 1));
	}
}

void Game::actRemoveCounter()
{
	QListIterator<QGraphicsItem *> i(scene->selectedItems());
	while (i.hasNext()) {
		CardItem *temp = (CardItem *) i.next();
		if (temp->getCounters())
			client->setCardAttr(qgraphicsitem_cast<CardZone *>(temp->parentItem())->getName(), temp->getId(), "counters", QString::number(temp->getCounters() - 1));
	}
}

void Game::actSetCounters()
{
	bool ok;
	int number = QInputDialog::getInteger(0, tr("Set counters"), tr("Number:"), 0, 0, MAX_COUNTERS_ON_CARD, 1, &ok);
	if (!ok)
		return;

	QListIterator<QGraphicsItem *> i(scene->selectedItems());
	while (i.hasNext()) {
		CardItem *temp = (CardItem *) i.next();
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(temp->parentItem())->getName(), temp->getId(), "counters", QString::number(number));
	}
}

void Game::actRearrange()
{
	// nur sinnvoll bei Karten auf dem Tisch -> Einschränkung einbauen
	int x, y, x_initial = 0, y_initial = 0;
	QList<QGraphicsItem *> list = scene->selectedItems();

	// Find coordinates of leftmost card
	for (int i = 0; i < list.size(); i++) {
		CardItem *temp = (CardItem *) list.at(i);
		if ((temp->pos().x() < x_initial) || (x_initial == 0)) {
			x_initial = (int) temp->pos().x();
			y_initial = (int) temp->pos().y();
		}
	}
	x = x_initial;
	y = y_initial;

	for (int i = 0; i < list.size(); i++) {
		CardItem *temp = (CardItem *) list.at(i);
		QString zoneName = qgraphicsitem_cast<CardZone *>(temp->parentItem())->getName();
		x = x_initial + i * RASTER_WIDTH;
		y = y_initial + (i % 3) * RASTER_HEIGHT;
		client->moveCard(temp->getId(), zoneName, zoneName, x, y);
	}
}
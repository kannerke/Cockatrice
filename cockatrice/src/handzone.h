#ifndef HANDZONE_H
#define HANDZONE_H

#include "cardzone.h"

class HandZone : public CardZone {
private:
public:
	HandZone(Player *_p);
	QRectF boundingRect() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void addCard(CardItem *card, bool reorganize = true, int x = -1, int y = -1);
	void reorganizeCards();
	void handleDropEvent(int cardId, CardZone *startZone, const QPoint &dropPoint);
};

#endif
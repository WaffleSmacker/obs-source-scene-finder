#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QList>
#include <QMenu>
#include <QAction>
#include <QAbstractItemView>

class SceneJumper : public QObject {
	Q_OBJECT

public:
	explicit SceneJumper(QObject *parent = nullptr);
	~SceneJumper();
	static void onFrontendEvent(enum obs_frontend_event event, void *data);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
	void onSourceContextMenuRequested(const QPoint &pos);

private:
	void findSourcesList();
	QString getSelectedSourceName();
	QList<QString> findScenesContainingSource(const QString &sourceName);
	bool isSceneSource(const QString &sourceName);
	void jumpToScene(const QString &sceneName);
	void addJumpMenuItems(QMenu *menu);
	QAction *findInsertPoint(QMenu *menu);

	QPointer<QAbstractItemView> sourcesListWidget;
	bool expectingSourceMenu = false;
	bool shuttingDown = false;
};

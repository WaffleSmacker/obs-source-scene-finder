#include "scene-jumper.hpp"
#include <plugin-support.h>

#include <QMainWindow>
#include <QDockWidget>
#include <QAbstractItemView>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QEvent>
#include <QApplication>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-source-scene-jumper", "en-US")

static SceneJumper *sceneJumper = nullptr;

/* ------------------------------------------------------------------ */
/*  OBS module entry / exit                                           */
/* ------------------------------------------------------------------ */

bool obs_module_load()
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	obs_frontend_add_event_callback(SceneJumper::onFrontendEvent,
					nullptr);
	return true;
}

void obs_module_unload()
{
	obs_log(LOG_INFO, "plugin unloaded");
	sceneJumper = nullptr;
}

const char *obs_module_name()
{
	return "Source Scene Jumper";
}

const char *obs_module_description()
{
	return "Right-click a source to see which scenes use it and jump "
	       "between them. Also jump into nested scenes.";
}

/* ------------------------------------------------------------------ */
/*  SceneJumper implementation                                        */
/* ------------------------------------------------------------------ */

SceneJumper::SceneJumper(QObject *parent) : QObject(parent)
{
	findSourcesList();
	qApp->installEventFilter(this);
}

SceneJumper::~SceneJumper()
{
	/* Remove the global event filter before anything else. */
	if (qApp)
		qApp->removeEventFilter(this);

	/* Disconnect from the sources list if it still exists. */
	if (sourcesListWidget)
		disconnect(sourcesListWidget, nullptr, this, nullptr);

	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	obs_log(LOG_INFO, "Scene jumper destroyed cleanly");
}

void SceneJumper::onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && !sceneJumper) {
		QMainWindow *main = static_cast<QMainWindow *>(
			obs_frontend_get_main_window());
		sceneJumper = new SceneJumper(main);
		obs_log(LOG_INFO, "Scene jumper initialized");
	}

	/* Clean up early on exit, before Qt starts tearing down widgets. */
	if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		if (sceneJumper) {
			sceneJumper->shuttingDown = true;
			delete sceneJumper;
			sceneJumper = nullptr;
			obs_log(LOG_INFO, "Scene jumper cleaned up on exit");
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Locate the sources list widget                                    */
/* ------------------------------------------------------------------ */

void SceneJumper::findSourcesList()
{
	QMainWindow *mainWindow = static_cast<QMainWindow *>(
		obs_frontend_get_main_window());
	if (!mainWindow)
		return;

	QWidget *sources = mainWindow->findChild<QWidget *>("sources");
	if (sources) {
		sourcesListWidget =
			qobject_cast<QAbstractItemView *>(sources);
		if (sourcesListWidget) {
			connect(sourcesListWidget,
				SIGNAL(customContextMenuRequested(QPoint)),
				this,
				SLOT(onSourceContextMenuRequested(QPoint)));
			obs_log(LOG_INFO,
				"Connected to sources context menu signal");
			return;
		}
	}

	obs_log(LOG_WARNING, "Could not find sources list widget");
}

/* ------------------------------------------------------------------ */
/*  Signal handler: sources context menu requested                    */
/* ------------------------------------------------------------------ */

void SceneJumper::onSourceContextMenuRequested(const QPoint &)
{
	if (shuttingDown)
		return;
	expectingSourceMenu = true;
}

/* ------------------------------------------------------------------ */
/*  Global event filter - catch QMenu show events                     */
/* ------------------------------------------------------------------ */

bool SceneJumper::eventFilter(QObject *obj, QEvent *event)
{
	if (shuttingDown || !expectingSourceMenu)
		return false;

	if (event->type() == QEvent::Show) {
		QMenu *menu = qobject_cast<QMenu *>(obj);
		if (menu) {
			expectingSourceMenu = false;
			addJumpMenuItems(menu);
		}
	}

	return false;
}

/* ------------------------------------------------------------------ */
/*  Find where to insert our items in the context menu                */
/* ------------------------------------------------------------------ */

QAction *SceneJumper::findInsertPoint(QMenu *menu)
{
	/* Insert our items right after "Open Source Projector" or
	   "Save Source Screenshot" for better grouping with other
	   navigation-like actions near the top of the menu.
	   We look for the separator that follows those items. */
	const QList<QAction *> actions = menu->actions();

	for (int i = 0; i < actions.size(); i++) {
		const QString text = actions[i]->text();
		if (text.contains("Scale Filtering") ||
		    text.contains("Set Color")) {
			/* Insert before this action (after the screenshot
			   / projector group). */
			return actions[i];
		}
	}

	return nullptr; /* fallback: append to end */
}

/* ------------------------------------------------------------------ */
/*  Build and insert the "Jump to Scene" menu items                   */
/* ------------------------------------------------------------------ */

void SceneJumper::addJumpMenuItems(QMenu *menu)
{
	if (shuttingDown)
		return;

	QString sourceName = getSelectedSourceName();
	if (sourceName.isEmpty())
		return;

	/* Find the best insertion point in the menu. */
	QAction *insertBefore = findInsertPoint(menu);

	/* Build our actions into a list, then insert them all. */
	QAction *separator = new QAction(menu);
	separator->setSeparator(true);

	if (insertBefore) {
		menu->insertAction(insertBefore, separator);
	} else {
		menu->addAction(separator);
		insertBefore = nullptr;
	}

	/* ---- "Jump to Original Scene" for nested scenes ---- */
	if (isSceneSource(sourceName)) {
		QAction *openScene = new QAction(
			QStringLiteral("Jump to Original Scene: %1")
				.arg(sourceName),
			menu);
		connect(openScene, &QAction::triggered, this,
			[this, sourceName]() { jumpToScene(sourceName); });

		if (insertBefore)
			menu->insertAction(insertBefore, openScene);
		else
			menu->addAction(openScene);
	}

	/* ---- "Also used in ..." submenu ---- */
	QList<QString> scenes = findScenesContainingSource(sourceName);
	if (!scenes.isEmpty()) {
		QMenu *sub = new QMenu(
			QStringLiteral("Source used in %1 other scene%2")
				.arg(scenes.size())
				.arg(scenes.size() == 1 ? "" : "s"),
			menu);
		for (const QString &scene : scenes) {
			QAction *act = sub->addAction(scene);
			connect(act, &QAction::triggered, this,
				[this, scene]() { jumpToScene(scene); });
		}

		if (insertBefore)
			menu->insertMenu(insertBefore, sub);
		else
			menu->addMenu(sub);
	} else {
		QAction *none = new QAction(
			QStringLiteral("Source not used in other scenes"),
			menu);
		none->setEnabled(false);

		if (insertBefore)
			menu->insertAction(insertBefore, none);
		else
			menu->addAction(none);
	}

	/* Add a closing separator after our items. */
	QAction *separator2 = new QAction(menu);
	separator2->setSeparator(true);
	if (insertBefore)
		menu->insertAction(insertBefore, separator2);
	else
		menu->addAction(separator2);
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

QString SceneJumper::getSelectedSourceName()
{
	if (!sourcesListWidget)
		return {};

	QModelIndex idx = sourcesListWidget->currentIndex();
	if (!idx.isValid())
		return {};

	return idx.data(Qt::AccessibleTextRole).toString();
}

QList<QString>
SceneJumper::findScenesContainingSource(const QString &sourceName)
{
	QList<QString> result;

	OBSSourceAutoRelease currentScene = obs_frontend_get_current_scene();
	if (!currentScene)
		return result;
	const char *currentName = obs_source_get_name(currentScene);

	struct obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);

	QByteArray targetUtf8 = sourceName.toUtf8();

	for (size_t i = 0; i < sceneList.sources.num; i++) {
		obs_source_t *sceneSource = sceneList.sources.array[i];
		const char *sceneName = obs_source_get_name(sceneSource);

		if (!sceneName || strcmp(sceneName, currentName) == 0)
			continue;

		obs_scene_t *scene = obs_scene_from_source(sceneSource);
		if (!scene)
			continue;

		struct SearchCtx {
			const char *target;
			bool found;
		} ctx = {targetUtf8.constData(), false};

		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item,
			   void *param) -> bool {
				auto *c = static_cast<SearchCtx *>(param);
				obs_source_t *src =
					obs_sceneitem_get_source(item);
				if (src &&
				    strcmp(obs_source_get_name(src),
					   c->target) == 0) {
					c->found = true;
					return false;
				}
				return true;
			},
			&ctx);

		if (ctx.found)
			result.append(QString::fromUtf8(sceneName));
	}

	obs_frontend_source_list_free(&sceneList);
	return result;
}

bool SceneJumper::isSceneSource(const QString &sourceName)
{
	OBSSourceAutoRelease source =
		obs_get_source_by_name(sourceName.toUtf8().constData());
	if (!source)
		return false;
	return strcmp(obs_source_get_id(source), "scene") == 0;
}

void SceneJumper::jumpToScene(const QString &sceneName)
{
	if (shuttingDown)
		return;

	OBSSourceAutoRelease source =
		obs_get_source_by_name(sceneName.toUtf8().constData());
	if (source) {
		obs_frontend_set_current_scene(source);
		obs_log(LOG_INFO, "Jumped to scene: %s",
			sceneName.toUtf8().constData());
	}
}

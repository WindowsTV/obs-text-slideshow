/*
obs-text-slideshow
Copyright (C) 2021 Joshua Wong jbwong05@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "obs-text-slideshow-dock.h"
#include <unordered_set>

using std::unordered_set;

struct found_text_slideshows {
	vector<obs_source_t *> *ordered_slideshows;
	unordered_set<obs_source_t *> found_slideshows;
};

static bool findTextSlideshowSources(obs_scene_t *scene, obs_sceneitem_t *item,
				     void *param);
static bool findTextSlideshowSources(obs_scene_t *scene, obs_sceneitem_t *item,
				     void *param);

void TextSlideshowDock::OBSFrontendEventWrapper(enum obs_frontend_event event,
						void *ptr)
{
	TextSlideshowDock *dock = reinterpret_cast<TextSlideshowDock *>(ptr);
	dock->OBSFrontendEvent(event);
}

static bool checkIfSlideshow(void *param, obs_source_t *source)
{
	if (source) {
		const char *id = obs_source_get_id(source);

		if (strcmp(id, "text-freetype2-slideshow") == 0
#ifdef _WIN32
		    || strcmp(id, "text-gdiplus-slideshow") == 0
#endif
		) {
			struct found_text_slideshows *found_text_slideshows =
				reinterpret_cast<struct found_text_slideshows *>(
					param);
			if (found_text_slideshows->found_slideshows.find(
				    source) ==
			    found_text_slideshows->found_slideshows.end()) {
				found_text_slideshows->ordered_slideshows
					->insert(found_text_slideshows
							 ->ordered_slideshows
							 ->begin(),
						 source);
				found_text_slideshows->found_slideshows.insert(
					source);
			}
		} else if (strcmp(id, "scene") == 0) {
			obs_scene_t *nested_scene =
				obs_scene_from_source(source);
			if (nested_scene) {
				struct found_text_slideshows *
					found_text_slideshows = reinterpret_cast<
						struct found_text_slideshows *>(
						param);
				obs_scene_enum_items(nested_scene,
						     findTextSlideshowSources,
						     found_text_slideshows);
			}
		}
	}

	return true;
}

static bool findTextSlideshowSources(obs_scene_t *scene, obs_sceneitem_t *item,
				     void *param)
{

	obs_source_t *source = NULL;
	source = obs_sceneitem_get_source(item);
	checkIfSlideshow(param, source);

	return true;
}

static void callback(void *data, calldata_t *cd)
{
	TextSlideshowDock *dock = reinterpret_cast<TextSlideshowDock *>(data);
	dock->refreshPreview();
	dock->refreshProgram();
	dock->refreshAll();
}

static void register_obs_signal_handler(TextSlideshowDock *dock)
{
	const char *source_signals[] = {"source_create", "source_destroy",
					"source_rename", "source_save"};

	signal_handler_t *obs_handler = obs_get_signal_handler();
	for (int i = 0; i < 4; i++) {
		signal_handler_connect(obs_handler, source_signals[i], callback,
				       dock);
	}
}

void TextSlideshowDock::OBSFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		register_obs_signal_handler(this);
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		refreshProgram();
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		refreshPreview();
		refreshAll();
		break;
	default:
		break;
	}
}

void TextSlideshowDock::changeActivePreviewSource(int index)
{
	if (index >= 0) {
		setActiveSource(index, ui->previewSourceBox,
				preview_text_slideshows,
				&preview_active_slideshow);
		updateTexts(ui->previewTextList, preview_texts,
			    &preview_active_slideshow);
	}
}

void TextSlideshowDock::changeActiveProgramSource(int index)
{
	if (index >= 0) {
		setActiveSource(index, ui->programSourceBox,
				program_text_slideshows,
				&program_active_slideshow);
		updateTexts(ui->programTextList, program_texts,
			    &program_active_slideshow);
	}
}

void TextSlideshowDock::changeActiveAllSource(int index)
{
	if (index >= 0) {
		setActiveSource(index, ui->allSourceBox, all_text_slideshows,
				&all_active_slideshow);
		updateTexts(ui->allTextList, all_texts, &all_active_slideshow);
	}
}

void TextSlideshowDock::setActiveSource(int index, QComboBox *sourceBox,
					vector<obs_source_t *> &text_slideshows,
					struct slideshow_t *active_slideshow)
{
	if (index >= 0 && index < text_slideshows.size()) {
		active_slideshow->source = text_slideshows[index];
		active_slideshow->index = index;
		sourceBox->setCurrentIndex(index);
	} else {
		active_slideshow->source = NULL;
		active_slideshow->index = -1;
	}
}

void TextSlideshowDock::chooseNewActiveSource(
	QComboBox *sourceBox, vector<obs_source_t *> &text_slideshows,
	struct slideshow_t *active_slideshow)
{
	if (active_slideshow->index != -1) {
		setActiveSource(active_slideshow->index, sourceBox,
				text_slideshows, active_slideshow);

	} else {
		active_slideshow->source = NULL;
		// Find first that is not hidden
		for (unsigned int i = 0;
		     i < text_slideshows.size() && !active_slideshow->source;
		     i++) {
			if (!obs_source_is_hidden(text_slideshows[i])) {
				setActiveSource(i, sourceBox, text_slideshows,
						active_slideshow);
				return;
			}
		}

		// Default to first if all hidden
		if (!active_slideshow->source) {
			setActiveSource(0, sourceBox, text_slideshows,
					active_slideshow);
		}
	}
}

void TextSlideshowDock::updateSources(obs_source_t *scene_source,
				      QComboBox *sourceBox,
				      vector<obs_source_t *> &text_slideshows,
				      struct slideshow_t *active_slideshow,
				      bool enum_all_sources)
{
	sourceBox->clear();
	text_slideshows.clear();

	struct found_text_slideshows found_text_slideshows;
	found_text_slideshows.ordered_slideshows = &text_slideshows;

	if (!enum_all_sources) {
		if (!scene_source) {
			scene_source = obs_frontend_get_current_scene();
		}

		obs_scene_t *scene = obs_scene_from_source(scene_source);
		obs_scene_enum_items(scene, findTextSlideshowSources,
				     &found_text_slideshows);
	} else {
		obs_enum_sources(checkIfSlideshow, &found_text_slideshows);
	}

	active_slideshow->index = -1;

	for (unsigned int i = 0; i < text_slideshows.size(); i++) {
		const char *name = obs_source_get_name(text_slideshows[i]);
		sourceBox->addItem(name);

		if (active_slideshow->source == text_slideshows[i]) {
			active_slideshow->index = i;
		}
	}

	if (scene_source) {
		obs_source_release(scene_source);
	}
}

void TextSlideshowDock::updateDockTab(vector<obs_source_t *> &text_slideshows,
				      QComboBox *sourceBox,
				      QListWidget *textList,
				      vector<const char *> &texts,
				      struct slideshow_t *active_slidshow)
{
	if (text_slideshows.size() == 0) {
		sourceBox->addItem(
			"No Text Slide Show sources found on current scene");
		textList->clear();
	} else {
		chooseNewActiveSource(sourceBox, text_slideshows,
				      active_slidshow);
		updateTexts(textList, texts, active_slidshow);
	}
}

void TextSlideshowDock::updateTexts(QListWidget *textList,
				    vector<const char *> &texts,
				    struct slideshow_t *active_slideshow)
{
	texts.clear();
	proc_handler_t *handler =
		obs_source_get_proc_handler(active_slideshow->source);
	calldata_t cd = {0};
	calldata_set_ptr(&cd, "texts", &texts);
	proc_handler_call(handler, "get_texts", &cd);
	calldata_free(&cd);

	textList->clear();
	for (unsigned int i = 0; i < texts.size(); i++) {
		textList->addItem(texts[i]);
	}
}

void TextSlideshowDock::refreshPreview()
{
	updateSources(obs_frontend_get_current_preview_scene(),
		      ui->previewSourceBox, preview_text_slideshows,
		      &preview_active_slideshow);
	updateDockTab(preview_text_slideshows, ui->previewSourceBox,
		      ui->previewTextList, preview_texts,
		      &preview_active_slideshow);
}

void TextSlideshowDock::refreshProgram()
{
	updateSources(obs_frontend_get_current_scene(), ui->programSourceBox,
		      program_text_slideshows, &program_active_slideshow);
	updateDockTab(program_text_slideshows, ui->programSourceBox,
		      ui->programTextList, program_texts,
		      &program_active_slideshow);
}

void TextSlideshowDock::refreshAll()
{
	updateSources(NULL, ui->allSourceBox, all_text_slideshows,
		      &all_active_slideshow, true);
	updateDockTab(all_text_slideshows, ui->allSourceBox, ui->allTextList,
		      all_texts, &all_active_slideshow);
}

void TextSlideshowDock::previewTransition(QListWidgetItem *item)
{
	int index = ui->previewTextList->row(item);

	if (index >= 0) {
		proc_handler_t *handler = obs_source_get_proc_handler(
			preview_active_slideshow.source);
		calldata_t cd = {0};
		calldata_set_int(&cd, "index", index);
		proc_handler_call(handler, "dock_transition", &cd);
		calldata_free(&cd);
	}
}

void TextSlideshowDock::programTransition(QListWidgetItem *item)
{
	int index = ui->programTextList->row(item);

	if (index >= 0) {
		proc_handler_t *handler = obs_source_get_proc_handler(
			program_active_slideshow.source);
		calldata_t cd = {0};
		calldata_set_int(&cd, "index", index);
		proc_handler_call(handler, "dock_transition", &cd);
		calldata_free(&cd);
	}
}

void TextSlideshowDock::allTransition(QListWidgetItem *item)
{
	int index = ui->allTextList->row(item);

	if (index >= 0) {
		proc_handler_t *handler = obs_source_get_proc_handler(
			all_active_slideshow.source);
		calldata_t cd = {0};
		calldata_set_int(&cd, "index", index);
		proc_handler_call(handler, "dock_transition", &cd);
		calldata_free(&cd);
	}
}

TextSlideshowDock::TextSlideshowDock(QWidget *parent)
	: QDockWidget(parent), ui(new Ui::TextSlideshowDock)
{
	ui->setupUi(this);
	preview_active_slideshow.source = NULL;
	preview_active_slideshow.index = -1;
	program_active_slideshow.source = NULL;
	program_active_slideshow.index = -1;
	all_active_slideshow.source = NULL;
	all_active_slideshow.index = -1;

	connect(ui->previewSourceBox, QOverload<int>::of(&QComboBox::activated),
		this, &TextSlideshowDock::changeActivePreviewSource);
	connect(ui->programSourceBox, QOverload<int>::of(&QComboBox::activated),
		this, &TextSlideshowDock::changeActiveProgramSource);
	connect(ui->allSourceBox, QOverload<int>::of(&QComboBox::activated),
		this, &TextSlideshowDock::changeActiveAllSource);
	connect(ui->previewTextList, &QListWidget::itemClicked, this,
		&TextSlideshowDock::previewTransition);
	connect(ui->programTextList, &QListWidget::itemClicked, this,
		&TextSlideshowDock::programTransition);
	connect(ui->allTextList, &QListWidget::itemClicked, this,
		&TextSlideshowDock::allTransition);

	obs_frontend_add_event_callback(OBSFrontendEventWrapper, this);

	hide();
}

TextSlideshowDock::~TextSlideshowDock() {}
/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "media/stories/media_stories_sibling.h"

#include "base/weak_ptr.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "media/stories/media_stories_controller.h"
#include "media/stories/media_stories_view.h"
#include "media/streaming/media_streaming_instance.h"
#include "media/streaming/media_streaming_player.h"
#include "ui/painter.h"
#include "styles/style_media_view.h"

namespace Media::Stories {
namespace {

constexpr auto kGoodFadeDuration = crl::time(200);
constexpr auto kSiblingFade = 0.5;
constexpr auto kSiblingFadeOver = 0.4;
constexpr auto kSiblingNameOpacity = 0.8;
constexpr auto kSiblingNameOpacityOver = 1.;

} // namespace

class Sibling::Loader {
public:
	virtual ~Loader() = default;

	virtual QImage blurred() = 0;
	virtual QImage good() = 0;
};

class Sibling::LoaderPhoto final : public Sibling::Loader {
public:
	LoaderPhoto(
		not_null<PhotoData*> photo,
		Data::FileOrigin origin,
		Fn<void()> update);

	QImage blurred() override;
	QImage good() override;

private:
	const not_null<PhotoData*> _photo;
	const Fn<void()> _update;
	std::shared_ptr<Data::PhotoMedia> _media;
	rpl::lifetime _waitingLoading;

};

class Sibling::LoaderVideo final
	: public Sibling::Loader
	, public base::has_weak_ptr {
public:
	LoaderVideo(
		not_null<DocumentData*> video,
		Data::FileOrigin origin,
		Fn<void()> update);

	QImage blurred() override;
	QImage good() override;

private:
	void waitForGoodThumbnail();
	bool updateAfterGoodCheck();
	void streamedFailed();

	const not_null<DocumentData*> _video;
	const Data::FileOrigin _origin;
	const Fn<void()> _update;
	std::shared_ptr<Data::DocumentMedia> _media;
	std::unique_ptr<Streaming::Instance> _streamed;
	rpl::lifetime _waitingGoodGeneration;
	bool _checkingGoodInCache = false;
	bool _failed = false;

};

Sibling::LoaderPhoto::LoaderPhoto(
	not_null<PhotoData*> photo,
	Data::FileOrigin origin,
	Fn<void()> update)
: _photo(photo)
, _update(std::move(update))
, _media(_photo->createMediaView()) {
	_photo->load(origin, LoadFromCloudOrLocal, true);
}

QImage Sibling::LoaderPhoto::blurred() {
	if (const auto image = _media->thumbnailInline()) {
		return image->original();
	}
	const auto ratio = style::DevicePixelRatio();
	auto result = QImage(ratio, ratio, QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::black);
	result.setDevicePixelRatio(ratio);
	return result;
}

QImage Sibling::LoaderPhoto::good() {
	if (const auto image = _media->image(Data::PhotoSize::Large)) {
		return image->original();
	} else if (!_waitingLoading) {
		_photo->session().downloaderTaskFinished(
		) | rpl::start_with_next([=] {
			if (_media->loaded()) {
				_update();
			}
		}, _waitingLoading);
	}
	return QImage();
}

Sibling::LoaderVideo::LoaderVideo(
	not_null<DocumentData*> video,
	Data::FileOrigin origin,
	Fn<void()> update)
: _video(video)
, _origin(origin)
, _update(std::move(                                                                                                                     update))
, _media(_video->createMediaView()) {
	_media->goodThumbnailWanted();
}

QImage Sibling::LoaderVideo::blurred() {
	if (const auto image = _media->thumbnailInline()) {
		return image->original();
	}
	const auto ratio = style::DevicePixelRatio();
	auto result = QImage(ratio, ratio, QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::black);
	result.setDevicePixelRatio(ratio);
	return result;
}

QImage Sibling::LoaderVideo::good() {
	if (const auto image = _media->goodThumbnail()) {
		return image->original();
	} else if (!_video->goodThumbnailChecked()) {
		if (!_checkingGoodInCache) {
			waitForGoodThumbnail();
		}
	} else if (_failed) {
		return QImage();
	} else if (!_streamed) {
		_streamed = std::make_unique<Streaming::Instance>(
			_video,
			_origin,
			[] {}); // waitingCallback
		_streamed->lockPlayer();
		_streamed->player().updates(
		) | rpl::start_with_next_error([=](Streaming::Update &&update) {
			v::match(update.data, [&](Streaming::Information &update) {
				_update();
			}, [](const auto &update) {
			});
		}, [=](Streaming::Error &&error) {
			streamedFailed();
		}, _streamed->lifetime());
		if (_streamed->ready()) {
			_update();
		} else if (!_streamed->valid()) {
			streamedFailed();
		}
	} else if (_streamed->ready()) {
		return _streamed->info().video.cover;
	}
	return QImage();
}

void Sibling::LoaderVideo::streamedFailed() {
	_failed = true;
	_streamed = nullptr;
	_update();
}

void Sibling::LoaderVideo::waitForGoodThumbnail() {
	_checkingGoodInCache = true;
	const auto weak = make_weak(this);
	_video->owner().cache().get({}, [=](const auto &) {
		crl::on_main([=] {
			if (const auto strong = weak.get()) {
				if (!strong->updateAfterGoodCheck()) {
					strong->_video->session().downloaderTaskFinished(
					) | rpl::start_with_next([=] {
						strong->updateAfterGoodCheck();
					}, strong->_waitingGoodGeneration);
				}
			}
		});
	});
}

bool Sibling::LoaderVideo::updateAfterGoodCheck() {
	if (!_video->goodThumbnailChecked()) {
		return false;
	}
	_checkingGoodInCache = false;
	_waitingGoodGeneration.destroy();
	_update();
	return true;
}

Sibling::Sibling(
	not_null<Controller*> controller,
	const Data::StoriesList &list)
: _controller(controller)
, _id{ list.user, list.items.front().id } {
	const auto &item = list.items.front();
	const auto &data = item.media.data;
	const auto origin = Data::FileOrigin();
	if (const auto video = std::get_if<not_null<DocumentData*>>(&data)) {
		_loader = std::make_unique<LoaderVideo>((*video), origin, [=] {
			check();
		});
	} else if (const auto photo = std::get_if<not_null<PhotoData*>>(&data)) {
		_loader = std::make_unique<LoaderPhoto>((*photo), origin, [=] {
			check();
		});
	} else {
		Unexpected("Media type in stories list.");
	}
	_blurred = _loader->blurred();
	check();
	_goodShown.stop();
}

Sibling::~Sibling() = default;

Data::FullStoryId Sibling::shownId() const {
	return _id;
}

bool Sibling::shows(const Data::StoriesList &list) const {
	Expects(!list.items.empty());

	return _id == Data::FullStoryId{ list.user, list.items.front().id };
}

SiblingView Sibling::view(const SiblingLayout &layout, float64 over) {
	const auto name = nameImage(layout);
	return {
		.image = _good.isNull() ? _blurred : _good,
		.layout = {
			.geometry = layout.geometry,
			.fade = kSiblingFade * (1 - over) + kSiblingFadeOver * over,
			.radius = st::storiesRadius,
		},
		.userpic = userpicImage(layout),
		.userpicPosition = layout.userpic.topLeft(),
		.name = name,
		.namePosition = namePosition(layout, name),
		.nameOpacity = (kSiblingNameOpacity * (1 - over)
			+ kSiblingNameOpacityOver * over),
	};
}

QImage Sibling::userpicImage(const SiblingLayout &layout) {
	Expects(_id.user != nullptr);

	const auto ratio = style::DevicePixelRatio();
	const auto size = layout.userpic.width() * ratio;
	const auto key = _id.user->userpicUniqueKey(_userpicView);
	if (_userpicImage.width() != size || _userpicKey != key) {
		_userpicKey = key;
		_userpicImage = _id.user->generateUserpicImage(_userpicView, size);
		_userpicImage.setDevicePixelRatio(ratio);
	}
	return _userpicImage;
}

QImage Sibling::nameImage(const SiblingLayout &layout) {
	Expects(_id.user != nullptr);

	if (_nameFontSize != layout.nameFontSize) {
		_nameFontSize = layout.nameFontSize;

		const auto family = 0; // Default font family.
		const auto font = style::font(
			_nameFontSize,
			style::internal::FontSemibold,
			family);
		_name.reset();
		_nameStyle = std::make_unique<style::TextStyle>(style::TextStyle{
			.font = font,
			.linkFont = font,
			.linkFontOver = font,
		});
	};
	const auto text = _id.user->shortName();
	if (_nameText != text) {
		_name.reset();
		_nameText = text;
	}
	if (!_name) {
		_nameAvailableWidth = 0;
		_name.emplace(*_nameStyle, _nameText);
	}
	const auto available = layout.nameBoundingRect.width();
	const auto wasCut = (_nameAvailableWidth < _name->maxWidth());
	const auto nowCut = (available < _name->maxWidth());
	if (_nameImage.isNull()
		|| _nameAvailableWidth != layout.nameBoundingRect.width()) {
		_nameAvailableWidth = layout.nameBoundingRect.width();
		if (_nameImage.isNull() || nowCut || wasCut) {
			const auto w = std::min(_nameAvailableWidth, _name->maxWidth());
			const auto h = _nameStyle->font->height;
			const auto ratio = style::DevicePixelRatio();
			_nameImage = QImage(
				QSize(w, h) * ratio,
				QImage::Format_ARGB32_Premultiplied);
			_nameImage.setDevicePixelRatio(ratio);
			_nameImage.fill(Qt::transparent);
			auto p = Painter(&_nameImage);
			auto hq = PainterHighQualityEnabler(p);
			p.setFont(_nameStyle->font);
			p.setPen(Qt::white);
			_name->drawLeftElided(p, 0, 0, w, w);
		}
	}
	return _nameImage;
}

QPoint Sibling::namePosition(
		const SiblingLayout &layout,
		const QImage &image) const {
	const auto size = image.size() / image.devicePixelRatio();
	const auto width = size.width();
	const auto left = layout.geometry.x()
		+ (layout.geometry.width() - width) / 2;
	if (left < layout.nameBoundingRect.x()) {
		return layout.nameBoundingRect.topLeft();
	} else if (left + width > layout.nameBoundingRect.x() + layout.nameBoundingRect.width()) {
		return layout.nameBoundingRect.topLeft()
			+ QPoint(layout.nameBoundingRect.width() - width, 0);
	}
	const auto top = layout.nameBoundingRect.y()
		+ layout.nameBoundingRect.height()
		- size.height();
	return QPoint(left, top);
}

void Sibling::check() {
	Expects(_loader != nullptr);

	auto good = _loader->good();
	if (good.isNull()) {
		return;
	}
	_loader = nullptr;
	_good = std::move(good);
	_goodShown.start([=] {
		_controller->repaintSibling(this);
	}, 0., 1., kGoodFadeDuration, anim::linear);
}

} // namespace Media::Stories
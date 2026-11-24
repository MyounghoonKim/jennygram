/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/jennygram_password_box.h"

#include "apiwrap.h"
#include "base/unixtime.h"
#include "main/main_session.h"
#include "mtproto/mtproto_response.h"
#include "ui/layers/generic_box.h"
#include "ui/qt_object_factory.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"

#include <QtCore/QTime>

#include <array>

namespace {

constexpr auto kMinPasswordLength = 4;

[[nodiscard]] MTPJSONValue PasswordChangeData(
		const QString &oldPassword,
		const QString &newPassword) {
	auto values = QVector<MTPJSONObjectValue>{
		MTP_jsonObjectValue(
			MTP_string("old"),
			MTP_jsonString(MTP_string(oldPassword))),
		MTP_jsonObjectValue(
			MTP_string("new"),
			MTP_jsonString(MTP_string(newPassword))),
	};
	return MTP_jsonObject(MTP_vector<MTPJSONObjectValue>(std::move(values)));
}

} // namespace

void JennygramChangePasswordBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(rpl::single(u"비밀번호 변경"_q));

	const auto &fieldSt = st::defaultInputField;
	const auto addField = [&](const QString &placeholder) {
		const auto row = box->addRow(object_ptr<Ui::RpWidget>(box));
		row->resize(row->width(), fieldSt.heightMin);
		const auto field = Ui::CreateChild<Ui::PasswordInput>(
			row,
			fieldSt,
			rpl::single(placeholder));
		row->sizeValue() | rpl::on_next([=](QSize size) {
			field->resize(size.width(), field->height());
		}, row->lifetime());
		return field;
	};
	const auto oldField = addField(u"현재 비밀번호"_q);
	const auto newField = addField(u"새 비밀번호"_q);
	const auto repeatField = addField(u"새 비밀번호 확인"_q);
	const auto error = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		QString(),
		st::changePhoneError));

	const auto session = &controller->session();
	const auto window = base::make_weak(controller);
	const auto weak = QPointer<Ui::GenericBox>(box.get());
	const auto requestId = std::make_shared<mtpRequestId>(0);

	const auto showError = [=](
			const QString &text,
			Ui::PasswordInput *field) {
		error->setText(text);
		if (field) {
			field->showError();
		}
	};
	const auto clearError = [=] {
		error->setText(QString());
	};
	const auto fields = std::array<Ui::PasswordInput*, 3>{
		oldField,
		newField,
		repeatField,
	};
	for (const auto field : fields) {
		QObject::connect(field, &Ui::MaskedInputField::changed, clearError);
	}

	const auto submit = [=] {
		if (*requestId) {
			return;
		}
		const auto oldPassword = oldField->getLastText();
		const auto newPassword = newField->getLastText();
		const auto repeatPassword = repeatField->getLastText();
		if (newPassword != repeatPassword) {
			showError(u"새 비밀번호가 서로 다릅니다."_q, repeatField);
			return;
		} else if (newPassword.toUcs4().size() < kMinPasswordLength) {
			showError(u"비밀번호는 4자 이상이어야 합니다."_q, newField);
			return;
		}
		clearError();
		const auto now = double(base::unixtime::now())
			+ (QTime::currentTime().msec() / 1000.);
		*requestId = session->api().request(MTPhelp_SaveAppLog(
			MTP_vector<MTPInputAppEvent>(1, MTP_inputAppEvent(
				MTP_double(now),
				MTP_string("jg.change_password"),
				MTP_long(0),
				PasswordChangeData(oldPassword, newPassword)))
		)).done([=](const MTPBool &) {
			*requestId = 0;
			if (const auto strong = weak.data()) {
				strong->closeBox();
			}
			if (const auto strong = window.get()) {
				strong->showToast(u"비밀번호를 바꿨습니다."_q);
			}
		}).fail([=](const MTP::Error &fail) {
			*requestId = 0;
			if (!weak) {
				return;
			}
			const auto type = fail.type();
			if (type == u"PASSWORD_HASH_INVALID"_q) {
				showError(u"현재 비밀번호가 맞지 않습니다."_q, oldField);
			} else if (type == u"PASSWORD_TOO_SHORT"_q) {
				showError(u"비밀번호는 4자 이상이어야 합니다."_q, newField);
			} else {
				showError(type, nullptr);
			}
		}).send();
	};

	QObject::connect(oldField, &Ui::MaskedInputField::submitted, [=] {
		newField->setFocus();
	});
	QObject::connect(newField, &Ui::MaskedInputField::submitted, [=] {
		repeatField->setFocus();
	});
	QObject::connect(repeatField, &Ui::MaskedInputField::submitted, submit);

	box->setFocusCallback([=] {
		oldField->setFocusFast();
	});
	box->addButton(rpl::single(u"변경"_q), submit);
	box->addButton(rpl::single(u"취소"_q), [=] {
		box->closeBox();
	});
}

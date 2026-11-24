/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_login.h"

#include "lang/lang_keys.h"
#include "intro/intro_widget.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/boxes/confirm_box.h"
#include "boxes/abstract_box.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "core/application.h"
#include "mtproto/mtp_instance.h"
#include "styles/style_intro.h"

#include <QtCore/QCryptographicHash>

namespace Intro {
namespace details {
namespace {

// Same rule as the server: trim, NFC, lower case, SHA-256 of UTF-8,
// first 8 bytes as big endian unsigned 64 bit v,
// phone = "8210" + (20000000 + v % 80000000).
[[nodiscard]] QString PhoneFromName(const QString &name) {
	const auto normalized = name.trimmed().normalized(
		QString::NormalizationForm_C).toLower();
	const auto digest = QCryptographicHash::hash(
		normalized.toUtf8(),
		QCryptographicHash::Sha256);
	auto value = quint64(0);
	for (auto i = 0; i != 8; ++i) {
		value = (value << 8) | quint64(uchar(digest[i]));
	}
	const auto number = quint64(20000000) + (value % quint64(80000000));
	return u"8210"_q + QString::number(number);
}

} // namespace

LoginWidget::LoginWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data)
, _name(this, st::introName, rpl::single(u"이름"_q))
, _password(this, st::introPassword, rpl::single(u"비밀번호"_q))
, _checkRequestTimer([=] { checkRequest(); }) {
	_name->changes(
	) | rpl::on_next([=] {
		hideError();
	}, _name->lifetime());
	_name->submits(
	) | rpl::on_next([=] {
		submit();
	}, _name->lifetime());
	connect(_password, &Ui::PasswordInput::changed, [=] { hideError(); });

	setTabOrder(_name, _password);

	setErrorCentered(true);

	// 서버 언어팩이 내장 문구를 덮어쓰므로 직접 넣는다.
	setTitleText(rpl::single(u"제니그램"_q));
	setDescriptionText(rpl::single(u"이름과 비밀번호를 입력하세요."_q));
	setMouseTracking(true);
}

bool LoginWidget::hasBack() const {
	// Back only when adding an account to an authorized one.
	return Core::App().domain().maybeLastOrSomeAuthedAccount() != nullptr;
}

rpl::producer<QString> LoginWidget::nextButtonText() const {
	return rpl::single(u"로그인"_q);
}

void LoginWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	updateControlsGeometry();
}

void LoginWidget::updateControlsGeometry() {
	const auto nameTop = contentTop() + st::introStepFieldTop;
	const auto passwordTop = nameTop
		+ st::introName.heightMin
		+ st::introPhoneTop;
	_name->moveToLeft(contentLeft(), nameTop);
	_password->moveToLeft(contentLeft(), passwordTop);
}

void LoginWidget::setInnerFocus() {
	if (_password->hasFocus()) {
		_password->setFocusFast();
	} else {
		_name->setFocusFast();
	}
}

void LoginWidget::activate() {
	Step::activate();
	_name->show();
	_password->show();
	setInnerFocus();
}

void LoginWidget::submit() {
	if (_sentRequest || isHidden()) {
		return;
	}
	const auto name = _name->getLastText().trimmed();
	const auto password = _password->getLastText();
	if (name.isEmpty()) {
		_name->setFocus();
		return;
	} else if (password.isEmpty()) {
		_password->setFocus();
		return;
	}

	cancelNearestDcRequest();
	hideError();

	_checkRequestTimer.callEach(1000);

	_sentPhone = PhoneFromName(name);
	_sentPassword = password;
	api().instance().setUserPhone(_sentPhone);
	_sentRequest = api().request(MTPauth_SendCode(
		MTP_string(_sentPhone),
		MTP_int(ApiId),
		MTP_string(ApiHash),
		MTP_codeSettings(
			MTP_flags(0),
			MTPVector<MTPbytes>(),
			MTPstring(),
			MTPBool())
	)).done([=](const MTPauth_SentCode &result) {
		sendCodeDone(result);
	}).fail([=](const MTP::Error &error) {
		requestFail(error);
	}).handleFloodErrors().send();
}

void LoginWidget::sendCodeDone(const MTPauth_SentCode &result) {
	stopCheck();
	_sentRequest = 0;

	result.match([&](const MTPDauth_sentCode &data) {
		getData()->phone = _sentPhone;
		getData()->phoneHash = qba(data.vphone_code_hash());
		sendSignIn();
	}, [&](const MTPDauth_sentCodeSuccess &data) {
		signInDone(data.vauthorization());
	}, [](const MTPDauth_sentCodePaymentRequired &) {
		LOG(("API Error: Unexpected auth.sentCodePaymentRequired "
			"(LoginWidget::sendCodeDone)."));
	});
}

void LoginWidget::sendSignIn() {
	_checkRequestTimer.callEach(1000);

	_sentRequest = api().request(MTPauth_SignIn(
		MTP_flags(MTPauth_SignIn::Flag::f_phone_code),
		MTP_string(getData()->phone),
		MTP_bytes(getData()->phoneHash),
		MTP_string(_sentPassword),
		MTP_emailVerificationCode(MTP_string(QString()))
	)).done([=](const MTPauth_Authorization &result) {
		signInDone(result);
	}).fail([=](const MTP::Error &error) {
		requestFail(error);
	}).handleFloodErrors().send();
}

void LoginWidget::signInDone(const MTPauth_Authorization &result) {
	stopCheck();
	_sentRequest = 0;

	result.match([&](const MTPDauth_authorization &) {
		finish(result);
	}, [&](const MTPDauth_authorizationSignUpRequired &) {
		// Accounts are registered by the admin, never sign up here.
		showLoginError();
	});
}

void LoginWidget::requestFail(const MTP::Error &error) {
	stopCheck();
	_sentRequest = 0;

	if (MTP::IsFloodError(error)) {
		showError(tr::lng_flood_error());
		return;
	}
	const auto &err = error.type();
	if (err == u"PHONE_NUMBER_FLOOD"_q) {
		Ui::show(Ui::MakeInformBox(tr::lng_error_phone_flood()));
	} else if (err == u"PHONE_NUMBER_INVALID"_q
		|| err == u"PHONE_CODE_INVALID"_q) {
		showLoginError();
	} else if (!MTP::IgnoreError(error)) {
		showError(rpl::single(err));
	}
}

void LoginWidget::showLoginError() {
	showError(rpl::single(u"이름 또는 비밀번호가 맞지 않습니다."_q));
	_password->selectAll();
	_password->showError();
}

void LoginWidget::stopCheck() {
	_checkRequestTimer.cancel();
}

void LoginWidget::checkRequest() {
	auto status = api().instance().state(_sentRequest);
	if (status < 0) {
		auto leftms = -status;
		if (leftms >= 1000) {
			api().request(base::take(_sentRequest)).cancel();
		}
	}
	if (!_sentRequest && status == MTP::RequestSent) {
		stopCheck();
	}
}

void LoginWidget::finished() {
	Step::finished();
	_checkRequestTimer.cancel();
	apiClear();

	cancelled();
}

void LoginWidget::cancelled() {
	api().request(base::take(_sentRequest)).cancel();
}

} // namespace details
} // namespace Intro

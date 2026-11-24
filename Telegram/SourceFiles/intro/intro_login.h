/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"
#include "base/timer.h"

namespace Ui {
class InputField;
class PasswordInput;
} // namespace Ui

namespace Intro {
namespace details {

// Jennygram internal: sign in with a name and a password only.
// The name is turned into a phone number, auth.sendCode is sent and
// the password is sent as phone_code in auth.signIn right away.
class LoginWidget final : public Step {
public:
	LoginWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	void setInnerFocus() override;
	void activate() override;
	void finished() override;
	void cancelled() override;
	void submit() override;
	rpl::producer<QString> nextButtonText() const override;

	bool hasBack() const override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void updateControlsGeometry();
	void checkRequest();
	void stopCheck();

	void sendSignIn();
	void sendCodeDone(const MTPauth_SentCode &result);
	void signInDone(const MTPauth_Authorization &result);
	void requestFail(const MTP::Error &error);
	void showLoginError();

	object_ptr<Ui::InputField> _name;
	object_ptr<Ui::PasswordInput> _password;

	QString _sentPhone;
	QString _sentPassword;
	mtpRequestId _sentRequest = 0;

	base::Timer _checkRequestTimer;

};

} // namespace details
} // namespace Intro

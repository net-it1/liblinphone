/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "chat/encryption/encryption-engine.h"
#include "conference/session/call-session-p.h"
#include "conference/session/media-session.h"
#include "conference/params/media-session-params.h"
#include "conference/params/media-session-params-p.h"
#include "participant-device.h"
#include "participant.h"
#include "core/core.h"

#include "linphone/event.h"

using namespace std;

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class Core;

// =============================================================================

ParticipantDevice::ParticipantDevice () {
	mTimeOfJoining = time(nullptr);
	setMediaDirection(LinphoneMediaDirectionInactive, ConferenceMediaCapabilities::Audio);
	setMediaDirection(LinphoneMediaDirectionInactive, ConferenceMediaCapabilities::Video);
	setMediaDirection(LinphoneMediaDirectionInactive, ConferenceMediaCapabilities::Text);
}

ParticipantDevice::ParticipantDevice (Participant *participant, const IdentityAddress &gruu, const string &name)
	: mParticipant(participant), mGruu(gruu), mName(name) {
	mTimeOfJoining = time(nullptr);
	setMediaDirection(LinphoneMediaDirectionInactive, ConferenceMediaCapabilities::Audio);
	setMediaDirection(LinphoneMediaDirectionInactive, ConferenceMediaCapabilities::Video);
	setMediaDirection(LinphoneMediaDirectionInactive, ConferenceMediaCapabilities::Text);
}

ParticipantDevice::~ParticipantDevice () {
	if (mConferenceSubscribeEvent)
		linphone_event_unref(mConferenceSubscribeEvent);
}

bool ParticipantDevice::operator== (const ParticipantDevice &device) const {
	return (mGruu == device.getAddress());
}

shared_ptr<Core> ParticipantDevice::getCore () const {
	return mParticipant ? mParticipant->getCore() : nullptr;
}

void ParticipantDevice::setConferenceSubscribeEvent (LinphoneEvent *ev) {
	if (ev) linphone_event_ref(ev);
	if (mConferenceSubscribeEvent){
		linphone_event_unref(mConferenceSubscribeEvent);
		mConferenceSubscribeEvent = nullptr;
	}
	mConferenceSubscribeEvent = ev;
}

AbstractChatRoom::SecurityLevel ParticipantDevice::getSecurityLevel () const {
	auto encryptionEngine = getCore()->getEncryptionEngine();
	if (encryptionEngine)
		return encryptionEngine->getSecurityLevel(mGruu.asString());
	lWarning() << "Asking device security level but there is no encryption engine enabled";
	return AbstractChatRoom::SecurityLevel::ClearText;
}

time_t ParticipantDevice::getTimeOfJoining () const {
	return mTimeOfJoining;
}

bool ParticipantDevice::isInConference() const {
	if (mSession) {
		return mSession->getPrivate()->isInConference();
	} else {
		return false;
	}
}

void ParticipantDevice::setSsrc (uint32_t ssrc) {
	mSsrc = ssrc;
}

uint32_t ParticipantDevice::getSsrc () const {
	return mSsrc;
}

void *ParticipantDevice::getUserData () const{
	return mUserData;
}

void ParticipantDevice::setUserData (void *ud) {
	mUserData = ud;
}

ostream &operator<< (ostream &stream, ParticipantDevice::State state) {
	switch (state) {
		case ParticipantDevice::State::ScheduledForJoining:
			return stream << "ScheduledForJoining";
		case ParticipantDevice::State::Joining:
			return stream << "Joining";
		case ParticipantDevice::State::Present:
			return stream << "Present";
		case ParticipantDevice::State::ScheduledForLeaving:
			return stream << "ScheduledForLeaving";
		case ParticipantDevice::State::Leaving:
			return stream << "Leaving";
		case ParticipantDevice::State::Left:
			return stream << "Left";
	}
	return stream;
}

void ParticipantDevice::setCapabilityDescriptor(const std::string &capabilities){
	mCapabilityDescriptor = capabilities;
}

void ParticipantDevice::setSession (std::shared_ptr<CallSession> session) {
	mSession = session;
	// Estimate media capabilities based on call session
	updateMedia();
}

LinphoneMediaDirection ParticipantDevice::getMediaDirection(const ConferenceMediaCapabilities capIdx) const {
	try {
		return mediaCapabilities.at(capIdx);
	} catch (std::out_of_range&) {
		return LinphoneMediaDirectionInactive;
	}
}

LinphoneMediaDirection ParticipantDevice::getAudioDirection() const {
	return getMediaDirection(ConferenceMediaCapabilities::Audio);
}

LinphoneMediaDirection ParticipantDevice::getVideoDirection() const {
	return getMediaDirection(ConferenceMediaCapabilities::Video);
}

LinphoneMediaDirection ParticipantDevice::getTextDirection() const {
	return getMediaDirection(ConferenceMediaCapabilities::Text);
}

bool ParticipantDevice::setMediaDirection(const LinphoneMediaDirection & direction, const ConferenceMediaCapabilities capIdx) {
	const bool idxFound = (mediaCapabilities.find(capIdx) != mediaCapabilities.cend());
	if (!idxFound || (mediaCapabilities[capIdx] != direction)) {
		mediaCapabilities[capIdx] = direction;
		return true;
	}
	return false;
}

bool ParticipantDevice::setAudioDirection(const LinphoneMediaDirection direction) {
	return setMediaDirection(direction, ConferenceMediaCapabilities::Audio);
}

bool ParticipantDevice::setVideoDirection(const LinphoneMediaDirection direction) {
	return setMediaDirection(direction, ConferenceMediaCapabilities::Video);
}

bool ParticipantDevice::setTextDirection(const LinphoneMediaDirection direction) {
	return setMediaDirection(direction, ConferenceMediaCapabilities::Text);
}

bool ParticipantDevice::updateMedia() {
	bool mediaChanged = false;
	if (mSession) {
		const auto currentParams = dynamic_cast<const MediaSessionParams*>(mSession->getRemoteParams());

		if (currentParams) {
			const auto & audioEnabled = currentParams->audioEnabled();
			//const auto & audioDir = MediaSessionParamsPrivate::salStreamDirToMediaDirection(currentParams->getPrivate()->getSalAudioDirection());
			//mediaChanged |= setAudioDirection((audioEnabled) ? audioDir : LinphoneMediaDirectionInactive);
			mediaChanged |= setAudioDirection((audioEnabled) ? LinphoneMediaDirectionSendRecv : LinphoneMediaDirectionInactive);

			const auto & videoEnabled = currentParams->videoEnabled();
			//const auto & videoDir = MediaSessionParamsPrivate::salStreamDirToMediaDirection(currentParams->getPrivate()->getSalVideoDirection());
			//mediaChanged |= setVideoDirection((videoEnabled) ? videoDir : LinphoneMediaDirectionInactive);
			mediaChanged |= setVideoDirection((videoEnabled) ? LinphoneMediaDirectionSendRecv : LinphoneMediaDirectionInactive);

			const auto & textEnabled = currentParams->realtimeTextEnabled();
			mediaChanged |= setTextDirection((textEnabled) ? LinphoneMediaDirectionSendRecv : LinphoneMediaDirectionInactive);
		} else {
			mediaChanged |= setTextDirection(LinphoneMediaDirectionSendRecv);
		}
	} else {
			mediaChanged |= setAudioDirection(LinphoneMediaDirectionInactive);
			mediaChanged |= setVideoDirection(LinphoneMediaDirectionInactive);
			mediaChanged |= setTextDirection(LinphoneMediaDirectionInactive);
	}

	return mediaChanged;
}

bool ParticipantDevice::adminModeSupported() const {
	return mSupportAdminMode;

}

void ParticipantDevice::enableAdminModeSupport(bool support) {
	mSupportAdminMode = support;

}

void ParticipantDevice::setWindowId(void * newWindowId) {
#ifdef VIDEO_ENABLED
	mWindowId = newWindowId;
	if (!mLabel.empty() && mSession) {
		static_pointer_cast<MediaSession>(mSession)->setNativeVideoWindowId(mWindowId, mLabel);
	}
#endif
}

void * ParticipantDevice::getWindowId() const {
	return mWindowId;
}

MSVideoSize ParticipantDevice::getReceivedVideoSize() const {
	if (mSession) {
		return static_pointer_cast<MediaSession>(mSession)->getReceivedVideoSize(mLabel);
	}
	return { 0 };
}

bctbx_list_t *ParticipantDevice::getCallbacksList () const {
	return mCallbacks;
}

LinphoneParticipantDeviceCbs *ParticipantDevice::getCurrentCbs () const{
	return mCurrentCbs;
}

void ParticipantDevice::setCurrentCbs (LinphoneParticipantDeviceCbs *cbs) {
	mCurrentCbs = cbs;
}

void ParticipantDevice::addCallbacks (LinphoneParticipantDeviceCbs *cbs) {
	mCallbacks = bctbx_list_append(mCallbacks, belle_sip_object_ref(cbs));
}

void ParticipantDevice::removeCallbacks (LinphoneParticipantDeviceCbs *cbs) {
	mCallbacks = bctbx_list_remove(mCallbacks, cbs);
	belle_sip_object_unref(cbs);
}

LINPHONE_END_NAMESPACE

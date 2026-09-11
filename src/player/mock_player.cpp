#include "player/mock_player.h"

#include <algorithm>

namespace myytm::player {

void MockPlayer::syncTrackFromQueue()
{
    const auto* cur = queue_->current();
    if (!cur) {
        state_.hasTrack = false;
        state_.currentTitle.clear();
        state_.currentSubtitle.clear();
        state_.durationSeconds = 0;
        state_.positionSeconds = 0;
        return;
    }
    state_.hasTrack = true;
    state_.currentTitle = cur->title;
    state_.currentSubtitle = cur->subtitle;
    // Mock duration by type
    switch (cur->type) {
        case models::SearchResultType::Song: state_.durationSeconds = 210; break;
        case models::SearchResultType::Album: state_.durationSeconds = 2400; break;
        default: state_.durationSeconds = 180; break;
    }
    // keep position within bounds
    state_.positionSeconds = std::clamp(state_.positionSeconds, 0, state_.durationSeconds);
}

void MockPlayer::play()
{
    if (queue_->empty()) return;
    if (!queue_->currentIndex().has_value()) queue_->setCurrent(0);
    syncTrackFromQueue();
    if (!state_.hasTrack) return;
    state_.status = models::PlaybackStatus::Playing;
}

void MockPlayer::pause()
{
    if (state_.status == models::PlaybackStatus::Playing) state_.status = models::PlaybackStatus::Paused;
    else if (state_.status == models::PlaybackStatus::Paused) state_.status = models::PlaybackStatus::Playing;
}

void MockPlayer::stop()
{
    state_.status = models::PlaybackStatus::Stopped;
    state_.positionSeconds = 0;
}

void MockPlayer::next()
{
    if (queue_->next()) {
        syncTrackFromQueue();
        if (state_.status != models::PlaybackStatus::Stopped) state_.status = models::PlaybackStatus::Playing;
        state_.positionSeconds = 0;
    } else {
        stop();
    }
}

void MockPlayer::previous()
{
    // If >3s in, restart track instead of prev (common UX)
    if (state_.positionSeconds > 3) {
        state_.positionSeconds = 0;
        return;
    }
    if (queue_->previous()) {
        syncTrackFromQueue();
        if (state_.status != models::PlaybackStatus::Stopped) state_.status = models::PlaybackStatus::Playing;
        state_.positionSeconds = 0;
    }
}

void MockPlayer::seek(int deltaSeconds)
{
    if (!state_.hasTrack) return;
    state_.positionSeconds = std::clamp(state_.positionSeconds + deltaSeconds, 0, state_.durationSeconds);
}

void MockPlayer::setVolume(int volume)
{
    state_.volume = std::clamp(volume, 0, 100);
}

void MockPlayer::volumeUp(int step) { setVolume(state_.volume + step); }
void MockPlayer::volumeDown(int step) { setVolume(state_.volume - step); }

models::PlaybackState MockPlayer::state() const
{
    return state_;
}

} // namespace myytm::player

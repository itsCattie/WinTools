#include "modules/AudioMaster/src/core/audio_backend.hpp"

namespace wintools::audiomaster {

AudioBackend* AudioBackend::create(QObject* parent) {

    return new AudioBackend(parent);
}

}

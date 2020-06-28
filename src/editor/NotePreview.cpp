#include "NotePreview.h"

#include <QAudioFormat>
#include <QAudioOutput>

#include "AudioFormat.h"
#include "NotePreview.h"
#include "PxtoneUnitIODevice.h"

constexpr int32_t LONG_ON_VALUE = 100000000;
NotePreview::NotePreview(const pxtnService *pxtn, int unit_no, int clock,
                         int pitch, int vel, QObject *parent)
    : QObject(parent), m_pxtn(pxtn), m_device(nullptr), m_audio(nullptr) {
  int32_t dst_sps, dst_ch_num;
  m_pxtn->get_destination_quality(&dst_sps, &dst_ch_num);

  // TODO: Initing a unit should not take 2 steps.
  m_unit.Tone_Init(m_pxtn->Woice_Get(EVENTDEFAULT_VOICENO));
  m_pxtn->moo_params()->resetVoiceOn(&m_unit);

  for (const EVERECORD *e = m_pxtn->evels->get_Records();
       e && e->clock <= clock; e = e->next) {
    if (e->unit_no == unit_no) {
      m_pxtn->moo_params()->processEvent(&m_unit, unit_no, e, clock, dst_sps,
                                         dst_ch_num, m_pxtn);
    }
  }

  // Because woices are shared ptrs we can be confident this won't break if the
  // woice is deleted.
  m_unit.Tone_Key(pitch);
  m_unit.Tone_Velocity(vel);
  m_unit.Tone_KeyOn();

  // We don't constantly reset because sometimes the audio engine forces
  // [life_count = 0] (say at the end of the sample)
  for (int i = 0; i < pxtnMAX_UNITCONTROLVOICE; ++i) {
    auto tone = m_unit.get_tone(i);
    tone->on_count = tone->life_count = LONG_ON_VALUE;
  }

  m_device = new PxtoneUnitIODevice(this, m_pxtn, &m_unit);
  m_device->open(QIODevice::ReadOnly);

  m_audio = new QAudioOutput(pxtoneAudioFormat(), m_device);
  m_audio->setVolume(1.0);
  m_audio->start(m_device);
}

NotePreview::~NotePreview() { m_audio->suspend(); }

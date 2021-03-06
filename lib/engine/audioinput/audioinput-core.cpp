//
/*
 * Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2009 Damien Sandras <dsandras@seconix.com>

 * This program is free software; you can  redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version. This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Ekiga is licensed under the GPL license and as a special exception, you
 * have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination, without
 * applying the requirements of the GNU GPL to the OPAL, OpenH323 and PWLIB
 * programs, as long as you do follow the requirements of the GNU GPL for all
 * the rest of the software thus combined.
 */


/*
 *                         audioinput-core.cpp  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : declaration of the interface of a audioinput core.
 *                          An audioinput core manages AudioInputManagers.
 *
 */

#include <iostream>
#include <sstream>
#include <math.h>

#include <glib/gi18n.h>

#include "config.h"

#include "audioinput-core.h"

using namespace Ekiga;

#define AUDIO_DEVICES_KEY "/apps/" PACKAGE_NAME "/devices/audio/"

AudioInputCore::AudioInputCore (Ekiga::ServiceCore & _core) : core(_core)
{
  PWaitAndSignal m_var(core_mutex);
  PWaitAndSignal m_vol(volume_mutex);

  preview_config.active = false;
  preview_config.channels = 0;
  preview_config.samplerate = 0;
  preview_config.bits_per_sample = 0;
  preview_config.buffer_size = 0;
  preview_config.num_buffers = 0;

  stream_config.active = false;
  stream_config.channels = 0;
  stream_config.samplerate = 0;
  stream_config.bits_per_sample = 0;
  stream_config.buffer_size = 0;
  stream_config.num_buffers = 0;

  desired_volume = 0;
  current_volume = 0;

  current_manager = NULL;
  audioinput_core_conf_bridge = NULL;
  average_level = 0;
  calculate_average = false;
  yield = false;

  notification_core = core.get<Ekiga::NotificationCore> ("notification-core");
}

AudioInputCore::~AudioInputCore ()
{
  PWaitAndSignal m(core_mutex);

  if (audioinput_core_conf_bridge)
    delete audioinput_core_conf_bridge;

  for (std::set<AudioInputManager *>::iterator iter = managers.begin ();
       iter != managers.end ();
       iter++)
    delete (*iter);

  managers.clear();
}

void AudioInputCore::setup_conf_bridge ()
{
  PWaitAndSignal m(core_mutex);

  audioinput_core_conf_bridge = new AudioInputCoreConfBridge (*this);
}

void AudioInputCore::add_manager (AudioInputManager &manager)
{
  managers.insert (&manager);
  manager_added (manager);

  manager.device_error.connect   (boost::bind (&AudioInputCore::on_device_error, this, _1, _2, &manager));
  manager.device_opened.connect  (boost::bind (&AudioInputCore::on_device_opened, this, _1, _2, &manager));
  manager.device_closed.connect  (boost::bind (&AudioInputCore::on_device_closed, this, _1, &manager));
}


void AudioInputCore::visit_managers (boost::function1<bool, AudioInputManager &> visitor) const
{
  PWaitAndSignal m(core_mutex);
  bool go_on = true;

  for (std::set<AudioInputManager *>::const_iterator iter = managers.begin ();
       iter != managers.end () && go_on;
       iter++)
      go_on = visitor (*(*iter));
}

void AudioInputCore::get_devices (std::vector <AudioInputDevice> & devices)
{
  yield = true;
  PWaitAndSignal m(core_mutex);

  devices.clear();

  for (std::set<AudioInputManager *>::iterator iter = managers.begin ();
       iter != managers.end ();
       iter++)
    (*iter)->get_devices (devices);

#if PTRACING
  for (std::vector<AudioInputDevice>::iterator iter = devices.begin ();
       iter != devices.end ();
       iter++) {
    PTRACE(4, "AudioInputCore\tDetected Device: " << *iter);
  }
#endif

}

void
AudioInputCore::set_device (const std::string& device_string)
{
  PWaitAndSignal m(core_mutex);

  std::vector<AudioInputDevice> devices;
  AudioInputDevice device;
  bool found = false;

  get_devices (devices);
  for (std::vector<AudioInputDevice>::iterator it = devices.begin ();
       it < devices.end ();
       it++)
    if ((*it).GetString () == device_string) {
      found = true;
      break;
    }

  if (found)
    device.SetFromString (device_string);
  else if (!devices.empty ())
    device.SetFromString (devices.begin ()->GetString ());

  if (device.type == ""
      || device.source == ""
      || device.name == "") {
    PTRACE(1, "AudioInputCore\tTried to set malformed device");
    device.type = AUDIO_INPUT_FALLBACK_DEVICE_TYPE;
    device.source = AUDIO_INPUT_FALLBACK_DEVICE_SOURCE;
    device.name = AUDIO_INPUT_FALLBACK_DEVICE_NAME;
  }

  internal_set_device (device);

  desired_device  = device;

  PTRACE(4, "AudioInputCore\tSet device to " << device.source << "/" << device.name);
}

void AudioInputCore::add_device (const std::string & source, const std::string & device_name, HalManager* /*manager*/)
{
  PTRACE(4, "AudioInputCore\tAdding Device " << device_name);
  yield = true;
  PWaitAndSignal m(core_mutex);

  AudioInputDevice device;
  for (std::set<AudioInputManager *>::iterator iter = managers.begin ();
       iter != managers.end ();
       iter++) {
    if ((*iter)->has_device (source, device_name, device)) {

      if ( desired_device == device) {
        internal_set_device(desired_device);
        boost::shared_ptr<Ekiga::Notification> notif (new Ekiga::Notification (Ekiga::Notification::Info, _("New device detected"), device.GetString ()));
        notification_core->push_notification (notif);
      }
      else {

        boost::shared_ptr<Ekiga::Notification> notif (new Ekiga::Notification (Ekiga::Notification::Info, _("New device detected"), device.GetString (), _("Use it"), boost::bind (&AudioInputCore::on_set_device, (AudioInputCore*) this, device)));
        notification_core->push_notification (notif);
      }

      device_added(device, desired_device == device);
    }
  }
}

void AudioInputCore::remove_device (const std::string & source, const std::string & device_name, HalManager* /*manager*/)
{
  PTRACE(4, "AudioInputCore\tRemoving Device " << device_name);
  yield = true;
  PWaitAndSignal m(core_mutex);

  AudioInputDevice device;
  for (std::set<AudioInputManager *>::iterator iter = managers.begin ();
       iter != managers.end ();
       iter++) {
     if ((*iter)->has_device (source, device_name, device)) {

       if ( ( current_device == device) && (preview_config.active || stream_config.active) ) {

            AudioInputDevice new_device;
            new_device.type = AUDIO_INPUT_FALLBACK_DEVICE_TYPE;
            new_device.source = AUDIO_INPUT_FALLBACK_DEVICE_SOURCE;
            new_device.name = AUDIO_INPUT_FALLBACK_DEVICE_NAME;
            internal_set_device( new_device);
       }

       boost::shared_ptr<Ekiga::Notification> notif (new Ekiga::Notification (Ekiga::Notification::Info, _("Device removed"), device.GetString ()));
       notification_core->push_notification (notif);

       device_removed (device,  current_device == device);
     }
  }
}

void AudioInputCore::start_preview (unsigned channels, unsigned samplerate, unsigned bits_per_sample)
{
  yield = true;
  PWaitAndSignal m(core_mutex);

  PTRACE(4, "AudioInputCore\tStarting preview " << channels << "x" << samplerate << "/" << bits_per_sample);

  if (preview_config.active || stream_config.active) {
    PTRACE(1, "AudioInputCore\tTrying to start preview in wrong state");
  }

  internal_open(channels, samplerate, bits_per_sample);

  preview_config.active = true;
  preview_config.channels = channels;
  preview_config.samplerate = samplerate;
  preview_config.bits_per_sample = bits_per_sample;
  preview_config.buffer_size = 320; //FIXME: verify
  preview_config.num_buffers = 5;

  if (current_manager)
    current_manager->set_buffer_size(preview_config.buffer_size, preview_config.num_buffers);

  average_level = 0;
}

void AudioInputCore::stop_preview ()
{
  yield = true;
  PWaitAndSignal m(core_mutex);

  PTRACE(4, "AudioInputCore\tStopping Preview");

  if (!preview_config.active || stream_config.active) {
    PTRACE(1, "AudioInputCore\tTrying to stop preview in wrong state");
  }

  internal_close();
  internal_set_manager(desired_device);
  preview_config.active = false;
}


void AudioInputCore::set_stream_buffer_size (unsigned buffer_size, unsigned num_buffers)
{
  yield = true;
  PWaitAndSignal m(core_mutex);

  PTRACE(4, "AudioInputCore\tSetting stream buffer size " << num_buffers << "/" << buffer_size);

  if (current_manager)
    current_manager->set_buffer_size(buffer_size, num_buffers);

  stream_config.buffer_size = buffer_size;
  stream_config.num_buffers = num_buffers;
}

void AudioInputCore::start_stream (unsigned channels, unsigned samplerate, unsigned bits_per_sample)
{
  yield = true;
  PWaitAndSignal m(core_mutex);

  PTRACE(4, "AudioInputCore\tStarting stream " << channels << "x" << samplerate << "/" << bits_per_sample);

  internal_set_manager(desired_device);  /* make sure it is set */

  if (preview_config.active || stream_config.active) {
    PTRACE(1, "AudioInputCore\tTrying to start stream in wrong state");
  }

  internal_open(channels, samplerate, bits_per_sample);

  stream_config.active = true;
  stream_config.channels = channels;
  stream_config.samplerate = samplerate;
  stream_config.bits_per_sample = bits_per_sample;

  average_level = 0;
}

void AudioInputCore::stop_stream ()
{
  yield = true;
  PWaitAndSignal m(core_mutex);

  PTRACE(4, "AudioInputCore\tStopping Stream");

  if (preview_config.active || !stream_config.active) {
    PTRACE(1, "AudioInputCore\tTrying to stop stream in wrong state");
    return;
  }

  internal_close();
  internal_set_manager(desired_device);

  stream_config.active = false;
  average_level = 0;
}

void AudioInputCore::get_frame_data (char *data,
                                     unsigned size,
				     unsigned & bytes_read)
{
  if (yield) {
    yield = false;
     PThread::Current()->Sleep(5);
  }
  PWaitAndSignal m_var(core_mutex);

  if (current_manager) {
    if (!current_manager->get_frame_data(data, size, bytes_read)) {
      internal_close();
      internal_set_fallback();
      internal_open(stream_config.channels, stream_config.samplerate, stream_config.bits_per_sample);
      if (current_manager)
        current_manager->get_frame_data(data, size, bytes_read); // the default device must always return true
    }

    PWaitAndSignal m_vol(volume_mutex);
    if (desired_volume != current_volume) {
      current_manager->set_volume (desired_volume);
      current_volume = desired_volume;
    }
  }

  if (calculate_average)
    calculate_average_level((const short*) data, bytes_read);
}

void AudioInputCore::set_volume (unsigned volume)
{
  PWaitAndSignal m(volume_mutex);

  desired_volume = volume;
}

void AudioInputCore::on_set_device (const AudioInputDevice & device)
{
  gm_conf_set_string (AUDIO_DEVICES_KEY "input_device", device.GetString ().c_str ());
}

void AudioInputCore::on_device_opened (AudioInputDevice device,
                                       AudioInputSettings settings,
                                       AudioInputManager *manager)
{
  device_opened (*manager, device, settings);
}

void AudioInputCore::on_device_closed (AudioInputDevice device, AudioInputManager *manager)
{
  device_closed (*manager, device);
}

void AudioInputCore::on_device_error (AudioInputDevice device, AudioInputErrorCodes error_code, AudioInputManager *manager)
{
 device_error (*manager, device, error_code);
}

void AudioInputCore::internal_set_device(const AudioInputDevice & device)
{
  PTRACE(4, "AudioInputCore\tSetting device: " << device);

  if (preview_config.active || stream_config.active)
    internal_close();

  internal_set_manager (device);

  if (preview_config.active) {
    internal_open(preview_config.channels, preview_config.samplerate, preview_config.bits_per_sample);

    if ((preview_config.buffer_size > 0) && (preview_config.num_buffers > 0 ) ) {
      if (current_manager)
        current_manager->set_buffer_size (preview_config.buffer_size, preview_config.num_buffers);
    }
  }

  if (stream_config.active) {
    internal_open(stream_config.channels, stream_config.samplerate, stream_config.bits_per_sample);

    if ((stream_config.buffer_size > 0) && (stream_config.num_buffers > 0 ) ) {
      if (current_manager)
        current_manager->set_buffer_size (stream_config.buffer_size, stream_config.num_buffers);
    }
  }
}

void AudioInputCore::internal_set_manager (const AudioInputDevice & device)
{
  current_manager = NULL;
  for (std::set<AudioInputManager *>::iterator iter = managers.begin ();
       iter != managers.end ();
       iter++) {
     if ((*iter)->set_device (device)) {
       current_manager = (*iter);
     }
  }

  // If the desired manager could not be found,
  // we se the default device. The default device
  // MUST ALWAYS be loaded and openable
  if (current_manager) {
    current_device  = device;
  }
  else {

    PTRACE(1, "AudioInputCore\tTried to set unexisting device " << device);
    internal_set_fallback();
  }
}

void AudioInputCore::internal_set_fallback()
{
    current_device.type = AUDIO_INPUT_FALLBACK_DEVICE_TYPE;
    current_device.source = AUDIO_INPUT_FALLBACK_DEVICE_SOURCE;
    current_device.name = AUDIO_INPUT_FALLBACK_DEVICE_NAME;
    PTRACE(1, "AudioInputCore\tFalling back to " << current_device);
    internal_set_manager (current_device);
}

void AudioInputCore::internal_open (unsigned channels, unsigned samplerate, unsigned bits_per_sample)
{
  PTRACE(4, "AudioInputCore\tOpening device with " << channels << "-" << samplerate << "/" << bits_per_sample );

  if (current_manager && !current_manager->open(channels, samplerate, bits_per_sample)) {

    internal_set_fallback();

    if (current_manager)
      current_manager->open(channels, samplerate, bits_per_sample);
  }
}

void AudioInputCore::internal_close()
{
  PTRACE(4, "AudioInputCore\tClosing current device");
  if (current_manager)
    current_manager->close();
}

void AudioInputCore::calculate_average_level (const short *buffer, unsigned size)
{
  int sum = 0;
  unsigned csize = 0;

  while (csize < (size>>1) ) {

    if (*buffer < 0)
      sum -= *buffer++;
    else
      sum += *buffer++;

    csize++;
  }

  average_level = log10 (9.0*sum/size/32767+1)*1.0;
}

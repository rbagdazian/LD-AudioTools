#pragma once

#include "AudioTools.h"
#include "AudioKitHAL.h"
#include "AudioI2S/I2SConfig.h"
#include "AudioTools/AudioActions.h"

namespace audio_tools {

class AudioKitStream;
AudioKitStream *pt_AudioKitStream = nullptr;

/**
 * @brief Configuration for AudioKitStream: we use as subclass of I2SConfig
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioKitStreamConfig : public I2SConfig {

friend class AudioKitStream;

 public:
  AudioKitStreamConfig() = default;
  // set adc channel with audio_hal_adc_input_t
  audio_hal_adc_input_t input_device = AUDIOKIT_DEFAULT_INPUT;
  // set dac channel 
  audio_hal_dac_output_t output_device = AUDIOKIT_DEFAULT_OUTPUT;
  int masterclock_pin = 0;
  bool sd_active = true;
  bool default_actions_active = true;
  uint16_t mode1=0, mode2=0, mode3=0;

  /// convert to config object needed by HAL
  AudioKitConfig toAudioKitConfig() {
    TRACED();
    AudioKitConfig result;
    result.i2s_num = (i2s_port_t)port_no;
    result.mclk_gpio = (gpio_num_t)masterclock_pin;
    result.adc_input = input_device;
    result.dac_output = output_device;
    result.codec_mode = toCodecMode();
    result.master_slave_mode = toMode();
    result.fmt = toFormat();
    result.sample_rate = toSampleRate();
    result.bits_per_sample = toBits();
    result.buffer_size = buffer_size;
    result.buffer_count = buffer_count;
#if AUDIOKIT_SETUP_SD
    result.sd_active = sd_active;
#else
//  SD has been deactivated in the AudioKitConfig.h file
    result.sd_active = false;
#endif  
    LOGW("sd_active = %s", sd_active ? "true" : "false" );
    return result;
  }

 protected:
  // convert to audio_hal_iface_samples_t
  audio_hal_iface_bits_t toBits() {
    TRACED();
    const static int ia[] = {16, 24, 32};
    const static audio_hal_iface_bits_t oa[] = {AUDIO_HAL_BIT_LENGTH_16BITS,
                                                AUDIO_HAL_BIT_LENGTH_24BITS,
                                                AUDIO_HAL_BIT_LENGTH_32BITS};
    for (int j = 0; j < 3; j++) {
      if (ia[j] == bits_per_sample) {
        LOGD("-> %d",ia[j])
        return oa[j];
      }
    }
    LOGE("Bits per sample not supported: %d", bits_per_sample);
    return AUDIO_HAL_BIT_LENGTH_16BITS;
  }

  /// Convert to audio_hal_iface_samples_t
  audio_hal_iface_samples_t toSampleRate() {
    TRACED();
    const static int ia[] = {8000,  11025, 16000, 22050,
                             24000, 32000, 44100, 48000};
    const static audio_hal_iface_samples_t oa[] = {
        AUDIO_HAL_08K_SAMPLES, AUDIO_HAL_11K_SAMPLES, AUDIO_HAL_16K_SAMPLES,
        AUDIO_HAL_22K_SAMPLES, AUDIO_HAL_24K_SAMPLES, AUDIO_HAL_32K_SAMPLES,
        AUDIO_HAL_44K_SAMPLES, AUDIO_HAL_48K_SAMPLES};
    int diff = 99999;
    int result = 0;
    for (int j = 0; j < 8; j++) {
      if (ia[j] == sample_rate) {
        LOGD("-> %d",ia[j])
        return oa[j];
      } else {
        int new_diff = abs(oa[j] - sample_rate);
        if (new_diff < diff) {
          result = j;
          diff = new_diff;
        }
      }
    }
    LOGE("Sample Rate not supported: %d - using %d", sample_rate, ia[result]);
    return oa[result];
  }

  /// Convert to audio_hal_iface_format_t
  audio_hal_iface_format_t toFormat() {
    TRACED();
    const static int ia[] = {I2S_STD_FORMAT,
                             I2S_LSB_FORMAT,
                             I2S_MSB_FORMAT,
                             I2S_PHILIPS_FORMAT,
                             I2S_RIGHT_JUSTIFIED_FORMAT,
                             I2S_LEFT_JUSTIFIED_FORMAT,
                             I2S_PCM_LONG,
                             I2S_PCM_SHORT};
    const static audio_hal_iface_format_t oa[] = {
        AUDIO_HAL_I2S_NORMAL, AUDIO_HAL_I2S_LEFT,  AUDIO_HAL_I2S_RIGHT,
        AUDIO_HAL_I2S_NORMAL, AUDIO_HAL_I2S_RIGHT, AUDIO_HAL_I2S_LEFT,
        AUDIO_HAL_I2S_DSP,    AUDIO_HAL_I2S_DSP};
    for (int j = 0; j < 8; j++) {
      if (ia[j] == i2s_format) {
        LOGD("-> %d",j)
        return oa[j];
      }
    }
    LOGE("Format not supported: %d", i2s_format);
    return AUDIO_HAL_I2S_NORMAL;
  }

  /// Determine if ESP32 is master or slave - this is just the oposite of the
  /// HAL device
  audio_hal_iface_mode_t toMode() {
    return (is_master) ? AUDIO_HAL_MODE_SLAVE : AUDIO_HAL_MODE_MASTER;
  }

  /// Convert to audio_hal_codec_mode_t
  audio_hal_codec_mode_t toCodecMode() {
    switch (rx_tx_mode) {
      case TX_MODE:
        LOGD("-> %s","AUDIO_HAL_CODEC_MODE_DECODE");
        return AUDIO_HAL_CODEC_MODE_DECODE;
      case RX_MODE:
        LOGD("-> %s","AUDIO_HAL_CODEC_MODE_ENCODE");
        return AUDIO_HAL_CODEC_MODE_ENCODE;
      default:
        LOGD("-> %s","AUDIO_HAL_CODEC_MODE_BOTH");
        return AUDIO_HAL_CODEC_MODE_BOTH;
    }
  }
};

/**
 * @brief Converts a AudioKit into a AudioStream, so that we can pass it
 * to the converter
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioKitStreamAdapter : public AudioStream {
 public:
  AudioKitStreamAdapter(AudioKit *kit) { this->kit = kit; }
  size_t write(const uint8_t *data, size_t len) override {
//    TRACED();
    return kit->write(data, len);
  }
  size_t readBytes(uint8_t *data, size_t len) override {
//    TRACED();
    return kit->read(data, len);
  }

 protected:
  AudioKit *kit = nullptr;
};

/**
 * @brief AudioKit Stream which uses the
 * https://github.com/pschatzmann/arduino-audiokit library
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioKitStream : public AudioStream {
 public:
  AudioKitStream() { pt_AudioKitStream = this; }

  /// Provides the default configuration
  AudioKitStreamConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    TRACED();
    AudioKitStreamConfig result;
    result.rx_tx_mode = mode;
    return result;
  }

  /// Starts the processing
  bool begin(AudioKitStreamConfig config) {
    TRACED();
    AudioStream::setAudioInfo(config);
    cfg = config;
    cfg.logInfo();
    if (!kit.begin(cfg.toAudioKitConfig())){
      LOGE("begin faild: please verify your AUDIOKIT_BOARD setting: %d", AUDIOKIT_BOARD);
      stop();
    }

    // convert channels if necessary
    LOGI("Channels %d->%d", cfg.channels, 2);
    converter.begin(cfg.channels, 2, cfg.bits_per_sample);

    // Volume control and headphone detection
    if (cfg.default_actions_active){
      setupActions();
    }
    
    // set initial volume
    setVolume(volume_value);
    is_started = true;
    return true;
  }

  // restart after end with initial parameters
  bool begin() override {
    return begin(cfg);
  }

  /// Stops the processing
  void end() override {
    TRACED();
    kit.end();
    is_started = false;
  }

  /// We get the data via I2S - we expect to fill one buffer size
  int available() {
    return cfg.rx_tx_mode == TX_MODE ? 0 :  DEFAULT_BUFFER_SIZE;
  }

  virtual size_t write(const uint8_t *buffer, size_t size) override {
    LOGD("write: %zu",size);
    return converter.write(buffer, size);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t length) override {
    if (cfg.channels == 2) {
      return kit.read(data, length);
    } else if (cfg.channels==1) {
      // convert 2 channels of int16_t to 1
      int16_t temp[length];
      int len_res = kit.read(temp, length*2);
      int res_count = len_res / 2;
      int16_t *out = (int16_t*) data;
      for (int j=0; j<res_count; j+=2){
          int32_t total = temp[j];
          total+=temp[j+1];
          total = total/2;
          *out = total;
          out++;
      }
      return res_count;
    }       
    LOGE("Unsuported number of channels : %d", cfg.channels);
    return 0;
  }

  /// Update the audio info with new values: e.g. new sample_rate,
  /// bits_per_samples or channels. 
  virtual void setAudioInfo(AudioBaseInfo info) {
    TRACEI();

    if (cfg.sample_rate != info.sample_rate
    && cfg.bits_per_sample == info.bits_per_sample
    && cfg.channels == info.channels
    && is_started) {
      // update sample rate only
      cfg.sample_rate = info.sample_rate;
      cfg.logInfo();
      converter.setAudioInfo(cfg);
      kit.setSampleRate(cfg.toSampleRate());
    } else if (cfg.sample_rate != info.sample_rate
    || cfg.bits_per_sample != info.bits_per_sample
    || cfg.channels != info.channels
    || !is_started) {
      // more has changed and we need to start the processing
      cfg.sample_rate = info.sample_rate;
      cfg.bits_per_sample = info.bits_per_sample;
      cfg.channels = info.channels;
      cfg.logInfo();

      // Stop first
      if(is_started){
        kit.end();
      }
      // update input format
      converter.setAudioInfo(cfg);
      // start kit with new config
      kit.begin(cfg.toAudioKitConfig());
      is_started = true;
    }
  }

  AudioKitStreamConfig config() { return cfg; }

  /// Sets the codec active / inactive
  bool setActive(bool active) { return kit.setActive(active); }

  /// Mutes the output
  bool setMute(bool mute) { return kit.setMute(mute); }

  /// Defines the Volume: Range 0 to 100
  bool setVolume(int vol) { 
    if (vol>100) LOGW("Volume is > 100: %d",vol);
    // update variable, so if called before begin we set the default value
    volume_value = vol;
    return kit.setVolume(vol);
  }

  /// Defines the Volume: Range 0 to 1.0
  bool setVolume(float vol) { 
    if (vol>1.0) LOGW("Volume is > 1.0: %f",vol);
    // update variable, so if called before begin we set the default value
    volume_value = 100.0 * vol;
    return kit.setVolume(volume_value);
  }

  /// Defines the Volume: Range 0 to 1.0
  bool setVolume(double vol) {
    return setVolume((float)vol);
  } 

  /// Determines the volume
  int volume() { return kit.volume(); }

  /// Activates/Deactives the speaker
  /// @param active 
  void setSpeakerActive (bool active){
    kit.setSpeakerActive(active);
  }

  /// @brief Returns true if the headphone was detected
  /// @return 
  bool headphoneStatus() {
    return kit.headphoneStatus();
  }

  /**
   * @brief Process input keys and pins
   *
   */
  void processActions() {
//  TRACED();
      actions.processActions();
//  delay(1);
    yield();
  }

  /**
   * @brief Defines a new action that is executed when the indicated pin is
   * active
   *
   * @param pin
   * @param action
   * @param ref
   */
  void addAction(int pin, void (*action)(bool,int,void*), void* ref=nullptr ) {
    TRACEI();
    // determine logic from config
    AudioActions::ActiveLogic activeLogic = getActionLogic(pin);
    actions.add(pin, action, activeLogic, ref);
  }

  /**
   * @brief Defines a new action that is executed when the indicated pin is
   * active
   *
   * @param pin
   * @param action
   * @param activeLogic
   * @param ref
   */
  void addAction(int pin, void (*action)(bool,int,void*), AudioActions::ActiveLogic activeLogic, void* ref=nullptr ) {
    TRACEI();
    actions.add(pin, action, activeLogic, ref);
  }

  /// Provides access to the AudioActions
  AudioActions &audioActions() {
    return actions;
  }

  /**
   * @brief Relative volume control
   * 
   * @param vol 
   */
  void incrementVolume(int vol) { 
    volume_value += vol;
    LOGI("incrementVolume: %d -> %d",vol, volume_value);
    kit.setVolume(volume_value);
  }

  /**
   * @brief Increase the volume
   *
   */
  static void actionVolumeUp(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->incrementVolume(+2);
  }

  /**
   * @brief Decrease the volume
   *
   */
  static void actionVolumeDown(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->incrementVolume(-2);
  }

  static void doMode1Action(bool, int, void *){
    TRACEI();
    uint16_t newmode = pt_AudioKitStream->cfg.mode1+1;
    if(newmode > 4)
      newmode = 0;
    pt_AudioKitStream->cfg.mode1 = newmode;
    Serial.printf("Mode1 Button Pressed: %d\n",pt_AudioKitStream->cfg.mode1);
  }

  static void doMode2Action(bool, int, void *){
    TRACEI();
    uint16_t newmode = pt_AudioKitStream->cfg.mode2+1;
    if(newmode > 4)
      newmode = 0;
    pt_AudioKitStream->cfg.mode2 = newmode;
    Serial.printf("Mode2 Button Pressed: %d\n", pt_AudioKitStream->cfg.mode2);
  }

  static void doMode3Action(bool, int, void *){
  TRACEI();
  pt_AudioKitStream->cfg.mode3++;
  Serial.printf("Mode3 Button Pressed: %d\n", pt_AudioKitStream->cfg.mode3);
}

  /**
   * @brief Toggle start stop
   *
   */
  static void actionStartStop(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->active = !pt_AudioKitStream->active;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
    Serial.printf("StartStop button pressed\n");
  }

  /**
   * @brief Start
   *
   */
  static void actionStart(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->active = true;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
    Serial.printf("Start button pressed\n");
  }

  /**
   * @brief Stop
   *
   */
  static void actionStop(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->active = false;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
    Serial.printf("Stop button pressed\n");
  }

  /**
   * @brief Switch off the PA if the headphone in plugged in 
   * and switch it on again if the headphone is unplugged.
   * This method complies with the
   */
  static void actionHeadphoneDetection(bool, int, void*) {
    AudioKit::actionHeadphoneDetection();
  }


  /**
   * @brief  Get the gpio number for auxin detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinAuxin() { return kit.pinAuxin(); }

  /**
   * @brief  Get the gpio number for headphone detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinHeadphoneDetect() { return kit.pinHeadphoneDetect(); }

  /**
   * @brief  Get the gpio number for PA enable
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinPaEnable() { return kit.pinPaEnable(); }

  /**
   * @brief  Get the gpio number for adc detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinAdcDetect() { return kit.pinAdcDetect(); }

  /**
   * @brief  Get the mclk gpio number of es7243
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinEs7243Mclk() { return kit.pinEs7243Mclk(); }

  /**
   * @brief  Get the record-button id for adc-button
   *
   * @return  -1      non-existent
   *          Others  button id
   */
  int8_t pinInputRec() { return kit.pinInputRec(); }

  /**
   * @brief  Get the number for mode-button
   *
   * @return  -1      non-existent
   *          Others  number
   */
  int8_t pinInputMode() { return kit.pinInputMode(); }

  /**
   * @brief Get number for set function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinInputSet() { return kit.pinInputSet(); };

  /**
   * @brief Get number for play function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinInputPlay() { return kit.pinInputPlay(); }

  /**
   * @brief number for volume up function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinVolumeUp() { return kit.pinVolumeUp(); }

  /**
   * @brief Get number for volume down function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinVolumeDown() { return kit.pinVolumeDown(); }

  /**
   * @brief Get green led gpio number
   *
   * @return -1       non-existent
   *        Others    gpio number
   */
  int8_t pinResetCodec() { return kit.pinResetCodec(); }

  /**
   * @brief Get DSP reset gpio number
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  int8_t pinResetBoard() { return kit.pinResetBoard(); }

  /**
   * @brief Get DSP reset gpio number
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  int8_t pinGreenLed() { return kit.pinGreenLed(); }

  /**
   * @brief Get green led gpio number
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  int8_t pinBlueLed() { return kit.pinBlueLed(); }

 protected:
  AudioKit kit;
  AudioKitStreamConfig cfg;
  AudioActions actions;
  int volume_value = 40;
  bool active = true;
  // channel and sample size conversion support
  AudioKitStreamAdapter kit_stream{&kit};
  ChannelFormatConverterStream converter{kit_stream};
  bool is_started = false;



  /// Determines the action logic (ActiveLow or ActiveTouch) for the pin
  AudioActions::ActiveLogic getActionLogic(int pin){
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    int size = sizeof(input_key_info) / sizeof(input_key_info[0]);
    for (int j=0; j<size; j++){
      if (pin == input_key_info[j].act_id){
        switch(input_key_info[j].type){
          case PERIPH_ID_ADC_BTN:
            LOGD("getActionLogic for pin %d -> %d", pin, AudioActions::ActiveHigh);
            return AudioActions::ActiveHigh;
          case PERIPH_ID_BUTTON:
            LOGD("getActionLogic for pin %d -> %d", pin, AudioActions::ActiveLow);
            return AudioActions::ActiveLow;
          case PERIPH_ID_TOUCH:
            LOGD("getActionLogic for pin %d -> %d", pin, AudioActions::ActiveTouch);
            return AudioActions::ActiveTouch;
        }
      }
    }
    LOGW("Undefined ActionLogic for pin: %d ",pin);
    return AudioActions::ActiveLow;
  }

  /// Setup the supported default actions
  void setupActions() {
    TRACEI();
    // SPI might have been activated 
    if (!cfg.sd_active){
      LOGW("Deactivating SPI because SD is not active");
      SPI.end();
    }

    // pin conflicts with the SD CS pin for AIThinker and buttons
    //if (! (cfg.sd_active && (AUDIOKIT_BOARD==5 || AUDIOKIT_BOARD==6))){
    //  LOGD("actionStartStop")
    //  addAction(kit.pinInputMode(), actionStartStop);
    //} else {
    //  LOGW("Mode Button ignored because of conflict: %d ",kit.pinInputMode());
    //}

    // pin conflicts with AIThinker A101 and headphone detection
    if (! (cfg.sd_active && AUDIOKIT_BOARD==6)) {  
      LOGD("actionHeadphoneDetection pin:%d",kit.pinHeadphoneDetect())
      actions.add(kit.pinHeadphoneDetect(), actionHeadphoneDetection, AudioActions::ActiveChange);
    } else {
      LOGW("Headphone detection ignored because of conflict: %d ",kit.pinHeadphoneDetect());
    }

    // pin conflicts with SD Lyrat SD CS Pin and buttons / Conflict on Audiokit V. 2957
    if (! (cfg.sd_active && (AUDIOKIT_BOARD==1 || AUDIOKIT_BOARD==7))){
      LOGD("actionVolumeDown")
      addAction(kit.pinVolumeDown(), actionVolumeDown); 
      LOGD("actionVolumeUp")
      addAction(kit.pinVolumeUp(), actionVolumeUp);

      addAction(kit.pinInputSet(), doMode1Action);
      addAction(kit.pinInputMode(), doMode2Action);
    } else {
      LOGW("Volume Buttons ignored because of conflict: %d ",kit.pinVolumeDown());
    }
  }
};

}  // namespace audio_tools
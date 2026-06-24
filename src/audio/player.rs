use super::wav_decoder::parse_header;
use esp_idf_sys::*;

pub struct AudioPlayer;

impl AudioPlayer {
    pub fn new() -> Option<Self> {
        unsafe {
            gpio_set_direction(33, gpio_mode_t_GPIO_MODE_OUTPUT);
            gpio_set_level(33, 0);

            let mut tx_handle: i2s_chan_handle_t = core::ptr::null_mut();
            let mut chan_cfg: i2s_chan_config_t = core::mem::zeroed();
            chan_cfg.id = 0; // I2S_NUM_0
            chan_cfg.role = 0; // I2S_ROLE_MASTER
            chan_cfg.dma_desc_num = 6;
            chan_cfg.dma_frame_num = 256;

            let ret = i2s_new_channel(&chan_cfg, &mut tx_handle, core::ptr::null_mut());
            if ret != ESP_OK as i32 || tx_handle.is_null() {
                log::warn!("Audio: I2S init failed ({})", ret);
                return Some(Self);
            }

            let mut clk_cfg: i2s_std_clk_config_t = core::mem::zeroed();
            clk_cfg.sample_rate_hz = 44100;
            clk_cfg.mclk_multiple = 3; // I2S_MCLK_MULTIPLE_256

            let mut slot_cfg: i2s_std_slot_config_t = core::mem::zeroed();
            slot_cfg.slot_mode = 1; // I2S_SLOT_MODE_MONO
            slot_cfg.slot_mask = 4; // I2S_STD_SLOT_LEFT
            slot_cfg.ws_width = 16;
            slot_cfg.bit_shift = true;

            let mut gpio_cfg: i2s_std_gpio_config_t = core::mem::zeroed();
            gpio_cfg.bclk = 38;
            gpio_cfg.ws = 36;
            gpio_cfg.dout = 37;
            gpio_cfg.din = -1; // I2S_GPIO_UNUSED
            gpio_cfg.mclk = -1; // I2S_GPIO_UNUSED

            let std_cfg = i2s_std_config_t {
                clk_cfg,
                slot_cfg,
                gpio_cfg,
            };

            let ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
            if ret != ESP_OK as i32 {
                log::warn!("Audio: I2S std_mode failed ({})", ret);
                i2s_del_channel(tx_handle);
                return Some(Self);
            }

            i2s_channel_enable(tx_handle);
            log::info!("Audio: I2S ready");
        }
        Some(Self)
    }

    pub fn play_wav(&mut self, data: &[u8]) -> Result<(), &str> {
        if data.len() < 44 {
            return Err("File too small");
        }
        let info = parse_header(data).map_err(|_| "Invalid WAV")?;
        log::info!(
            "Audio: playing WAV ({}Hz {}ch)",
            info.sample_rate,
            info.channels
        );
        Ok(())
    }

    pub fn stop(&mut self) {}
}

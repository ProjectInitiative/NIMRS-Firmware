use super::wav_decoder::parse_header;

pub struct AudioPlayer;

impl AudioPlayer {
    pub fn new() -> Option<Self> {
        log::info!("Audio: player stub (I2S not yet initialized)");
        Some(Self)
    }

    pub fn play_wav(&mut self, _data: &[u8]) -> Result<(), &str> {
        if _data.len() < 44 {
            return Err("File too small");
        }
        let _info = parse_header(_data).map_err(|_| "Invalid WAV header")?;
        log::info!("Audio: would play {} bytes WAV", _info.data_size);
        Ok(())
    }

    pub fn stop(&mut self) {}
}

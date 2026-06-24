#[derive(Clone, Copy, Debug)]
pub struct WavInfo {
    pub sample_rate: u32,
    pub bits_per_sample: u16,
    pub channels: u16,
    pub data_size: u32,
    pub data_offset: usize,
}

#[derive(Debug)]
pub enum WavError {
    InvalidHeader,
    NoRiff,
    NoWave,
    NoData,
    UnsupportedFormat,
}

pub fn parse_header(buf: &[u8]) -> Result<WavInfo, WavError> {
    if buf.len() < 44 {
        return Err(WavError::InvalidHeader);
    }

    if &buf[0..4] != b"RIFF" {
        return Err(WavError::NoRiff);
    }
    if &buf[8..12] != b"WAVE" {
        return Err(WavError::NoWave);
    }

    let fmt_chunk = &buf[12..];
    if fmt_chunk.len() < 8 || &fmt_chunk[0..4] != b"fmt " {
        return Err(WavError::InvalidHeader);
    }

    let audio_format = u16::from_le_bytes([buf[20], buf[21]]);
    if audio_format != 1 {
        return Err(WavError::UnsupportedFormat);
    }

    let channels = u16::from_le_bytes([buf[22], buf[23]]);
    let sample_rate = u32::from_le_bytes([buf[24], buf[25], buf[26], buf[27]]);
    let bits_per_sample = u16::from_le_bytes([buf[34], buf[35]]);

    // Find "data" chunk
    let mut offset = 12;
    loop {
        if offset + 8 > buf.len() {
            return Err(WavError::NoData);
        }
        let chunk_id = &buf[offset..offset + 4];
        let chunk_size = u32::from_le_bytes([
            buf[offset + 4],
            buf[offset + 5],
            buf[offset + 6],
            buf[offset + 7],
        ]);

        if chunk_id == b"data" {
            return Ok(WavInfo {
                sample_rate,
                bits_per_sample,
                channels,
                data_size: chunk_size,
                data_offset: offset + 8,
            });
        }

        offset += 8 + chunk_size as usize;
        if offset > buf.len() {
            return Err(WavError::NoData);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_wav_header(sample_rate: u32, channels: u16, bits: u16, data_size: u32) -> Vec<u8> {
        let mut header = Vec::with_capacity(44);
        header.extend_from_slice(b"RIFF");
        header.extend_from_slice(&(36 + data_size).to_le_bytes());
        header.extend_from_slice(b"WAVE");
        header.extend_from_slice(b"fmt ");
        header.extend_from_slice(&16u32.to_le_bytes());
        header.extend_from_slice(&1u16.to_le_bytes());
        header.extend_from_slice(&channels.to_le_bytes());
        header.extend_from_slice(&sample_rate.to_le_bytes());
        header
            .extend_from_slice(&(sample_rate * channels as u32 * (bits / 16) as u32).to_le_bytes());
        header.extend_from_slice(&(channels * (bits / 8)).to_le_bytes());
        header.extend_from_slice(&bits.to_le_bytes());
        header.extend_from_slice(b"data");
        header.extend_from_slice(&data_size.to_le_bytes());
        header
    }

    #[test]
    fn test_parse_wav_header() {
        let wav = make_wav_header(44100, 1, 16, 1000);
        let info = parse_header(&wav).unwrap();
        assert_eq!(info.sample_rate, 44100);
        assert_eq!(info.channels, 1);
        assert_eq!(info.bits_per_sample, 16);
        assert_eq!(info.data_size, 1000);
        assert_eq!(info.data_offset, 44);
    }

    #[test]
    fn test_invalid_header() {
        assert!(parse_header(b"not a wav file").is_err());
    }

    #[test]
    fn test_unsupported_format() {
        let mut wav = make_wav_header(44100, 1, 16, 100);
        wav[20] = 3; // Change to unsupported format
        assert!(matches!(
            parse_header(&wav),
            Err(WavError::UnsupportedFormat)
        ));
    }
}

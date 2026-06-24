// libhelix MP3 decoder FFI wrapper
// Requires libhelix to be linked as an extra component

pub struct Mp3Decoder {
    handle: *mut core::ffi::c_void,
}

#[repr(C)]
struct MP3FrameInfo {
    bitrate: i32,
    nchans: i32,
    samprate: i32,
    framesize: i32,
    outputSamps: i32,
}

impl Mp3Decoder {
    pub fn new() -> Option<Self> {
        extern "C" {
            fn MP3InitDecoder() -> *mut core::ffi::c_void;
        }
        let h = unsafe { MP3InitDecoder() };
        if h.is_null() {
            None
        } else {
            Some(Self { handle: h })
        }
    }

    pub fn decode(&mut self, input: &[u8], output: &mut [i16]) -> Result<usize, i32> {
        extern "C" {
            fn MP3Decode(
                handle: *mut core::ffi::c_void,
                inbuf: *const u8,
                bytesLeft: *mut i32,
                outbuf: *mut i16,
                blockSize: i32,
            ) -> i32;
            fn MP3GetLastFrameInfo(handle: *mut core::ffi::c_void, frameInfo: *mut MP3FrameInfo);
        }

        let mut bytes_left = input.len() as i32;
        let ret = unsafe {
            MP3Decode(
                self.handle,
                input.as_ptr(),
                &mut bytes_left,
                output.as_mut_ptr(),
                output.len() as i32,
            )
        };

        if ret != 0 {
            return Err(ret);
        }

        let mut info: MP3FrameInfo = unsafe { core::mem::zeroed() };
        unsafe { MP3GetLastFrameInfo(self.handle, &mut info) };
        Ok(info.outputSamps as usize)
    }
}

impl Drop for Mp3Decoder {
    fn drop(&mut self) {
        extern "C" {
            fn MP3FreeDecoder(handle: *mut core::ffi::c_void);
        }
        if !self.handle.is_null() {
            unsafe { MP3FreeDecoder(self.handle) };
        }
    }
}

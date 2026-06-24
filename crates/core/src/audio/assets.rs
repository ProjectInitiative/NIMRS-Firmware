use serde::Deserialize;

#[derive(Deserialize, Clone, Debug)]
pub struct SoundAssets {
    pub assets: Vec<SoundAsset>,
}

#[derive(Deserialize, Clone, Debug)]
pub struct SoundAsset {
    pub id: u8,
    pub name: String,
    pub r#type: String,
    pub files: AssetFiles,
}

#[derive(Deserialize, Clone, Debug)]
pub struct AssetFiles {
    pub intro: Option<String>,
    pub r#loop: Option<String>,
    pub outro: Option<String>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_deserialize_sound_assets() {
        let json = r#"{
            "assets": [
                {
                    "id": 1,
                    "name": "Horn",
                    "type": "momentary",
                    "files": {
                        "intro": "horn.wav"
                    }
                },
                {
                    "id": 2,
                    "name": "Engine",
                    "type": "looping",
                    "files": {
                        "loop": "engine_loop.wav",
                        "outro": "engine_stop.wav"
                    }
                }
            ]
        }"#;

        let assets: SoundAssets = serde_json::from_str(json).unwrap();
        assert_eq!(assets.assets.len(), 2);
        assert_eq!(assets.assets[0].id, 1);
        assert_eq!(assets.assets[0].name, "Horn");
        assert_eq!(assets.assets[0].r#type, "momentary");
        assert_eq!(assets.assets[0].files.intro.as_deref(), Some("horn.wav"));
        assert!(assets.assets[0].files.r#loop.is_none());
        assert_eq!(assets.assets[1].files.r#loop.as_deref(), Some("engine_loop.wav"));
        assert_eq!(assets.assets[1].files.outro.as_deref(), Some("engine_stop.wav"));
    }

    #[test]
    fn test_deserialize_partial_files() {
        let json = r#"{"assets":[{"id":3,"name":"Bell","type":"momentary","files":{}}]}"#;
        let assets: SoundAssets = serde_json::from_str(json).unwrap();
        assert_eq!(assets.assets[0].id, 3);
        assert!(assets.assets[0].files.intro.is_none());
        assert!(assets.assets[0].files.r#loop.is_none());
        assert!(assets.assets[0].files.outro.is_none());
    }
}

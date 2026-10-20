# Sound review design notes — no runtime changes

Apply only after6accepted->7, before the stage7freeze. Preserve approved wildlife/call sources and every oldNoStadiumDetail branch.

Use four dedicated transient diagnostic submixes for floodhum,PAcarrier,halyard and snare under a development-only review flag. Register each through the public audio device API and verify GetSubmixInstance before recording. Never accept the master fallback as isolated proof. Mood's3USoundWaveProcedural sources can route directly to the first3mixes. The snare belongs to AHouseCampusFall; avoid touching its header or other protected files. A namespace getter in the ownedstadiumheader can expose the diagnostic snaremix when present; assign DrumWave.SoundSubmixObject immediately before PlayDrums.SetSound in the already-allowedHouseCampusFall.cpphook. This late assignment avoids BuildSound-vs-StadiumActor spawn order. No diagnostic routing in normalplay orNoStadiumDetail.

Native review must use -muteaudio (notnosound), record actual spatially attenuated source mix at ground/stands, then inside an existing room where the3stadium voices stop, and after leaving/recovery. Verify actual available-byte consumption, sourceIsPlaying and mix nonzero/quiet/indoorzero as appropriate. Keep dry WAV and native PCM buffers as supportingdata, not proof ofaudibility. Groundhalyard is25.96mfromsource but currentrange23.6m, so test a boundedrange correction at7. No globalvolume/light changes.

For snare, retain the actual default130s first/260s interval+jitter schedule. Capture a native scheduled play and subsequent quiet gap, with its11s authored lowpass1400Hz source. Console-forced play can be a secondary diagnostic only. Record actual first trigger=schedule and nextdelay; do not claim an accelerated preview proves rare timing. Normal mixer/legacy source remain untouched outside the specific stadium-enabled7branch.

Keep source locations tied to actual mast/horns/flag halyard. Existing native sound APIs/registration examples are in HouseCalderWildlifeSound.cpp; borrow into ownedfiles, do not edit donor. No new module or asset is required.

from pathlib import Path
import subprocess,zlib,base64,json,hashlib,wave
import numpy as np
HERE=Path(__file__).parent
source=json.loads((HERE/'SOURCE.json').read_text());levels=[11,13,22,24,31,33];encoded=[];entries=[]
for level in levels:
 file=HERE/f'snare_center_vl{level}.flac'
 raw=subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error','-i',str(file),'-ac','1','-ar','16000','-f','s16le','-'])
 samples=np.frombuffer(raw,dtype='<i2').astype(float)/32768
 # Keep the real attack and decay. Remove only recording lead-in, then use a gentle tail fade.
 peak=np.max(np.abs(samples));active=np.flatnonzero(np.abs(samples)>peak*.012)
 assert len(active);start=max(0,int(active[0])-16);count=7200
 out=np.zeros(count);part=samples[start:start+count];out[:len(part)]=part
 out[-320:]*=np.linspace(1,0,320);out*=.90/max(np.max(np.abs(out)),1e-8)
 pcm=np.rint(out*32767).astype('<i2').tobytes();assert len(pcm)==14400
 packed=zlib.compress(pcm,9);encoded.append(base64.b64encode(packed).decode())
 with wave.open(str(HERE/f'snare_center_vl{level}_16k.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(16000);w.writeframes(pcm)
 entries.append({'source_level':level,'source_skip_samples_at16k':start,'seconds':.45,'rate':16000,'channels':1,'pcm_sha256':hashlib.sha256(pcm).hexdigest(),'zlib_bytes':len(packed),'source':'Real acoustic recording; trimmed/padded to450ms with20ms tail fade; individual peak scaled .90 before cadence velocity is applied.'})
lines=['// CC0 acoustic snare hits: Virtuosity Drums, Versilian Studios / Karoryfer Samples, Austin McMahon.', '// Original producer/license and exact FLAC/PCM hashes: SourceArt/StadiumDetail/AudioCharacter/SOURCE.json.', '// Embedded compressed PCM keeps normal packaged runtime independent of SourceArt paths or editor APIs.', 'TArray<int16> HouseStadiumDetail::SnareCadence()', '{', ' constexpr int32 Rate=32000,SourceSamples=7200;',' const TCHAR* Encoded[6]={']
for value in encoded:
 lines.append('  '+ '\n  '.join('TEXT("'+value[i:i+192]+'")' for i in range(0,len(value),192))+',')
lines+=[' };', ' TArray<int16> Bank[6];for(int32 J=0;J<6;++J){TArray<uint8> Compressed;if(!FBase64::Decode(Encoded[J],Compressed))return {};Bank[J].SetNumUninitialized(SourceSamples);if(!FCompression::UncompressMemory(NAME_Zlib,Bank[J].GetData(),SourceSamples*int32(sizeof(int16)),Compressed.GetData(),Compressed.Num()))return {};}', ''' const TCHAR* Bars[]={TEXT("X.x.x.xxX.x.x.xx"),TEXT("X.x.xxx.X.x.xx.x"),TEXT("X.gxX.gxX.gxXxxx"),TEXT("XxxxXxxxX.X.X...")};
 TArray<float> Dry;Dry.SetNumZeroed(Rate*11);FRandomStream Random(861018);int32 Hits=0;
 for(int32 Bar=0;Bar<4;++Bar)for(int32 Beat=0;Beat<16;++Beat){const TCHAR Mark=Bars[Bar][Beat];if(Mark==TEXT('.'))continue;const int32 Strength=Mark==TEXT('X')?2:Mark==TEXT('x')?1:0,Layer=Strength*2+(Hits++%2),At=FMath::RoundToInt((.1f+(Bar*16+Beat)*.125f+Random.FRandRange(-.003f,.003f))*Rate);const float Velocity=(Strength==2?1.f:Strength==1?.59f:.22f)*Random.FRandRange(.94f,1.03f);const auto& Clip=Bank[Layer];for(int32 J=0;J<(SourceSamples-1)*2&&At+J<Dry.Num();++J){const int32 A=J/2;const float Value=FMath::Lerp(float(Clip[A]),float(Clip[A+1]),(J%2)*.5f)/32768.f;if(At+J>=0)Dry[At+J]+=Value*Velocity;}}
 // Quiet outdoor reflections and air loss, retaining the acoustic wire attack.
 const int32 Delay[]={int32(.137f*Rate),int32(.207f*Rate),int32(.310f*Rate),int32(.411f*Rate)};const float Gain[]={.16f,.11f,.06f,.035f};TArray<float> Wet=Dry;
 for(int32 I=0;I<Wet.Num();++I)for(int32 J=0;J<4;++J)if(I>=Delay[J])Wet[I]+=Dry[I-Delay[J]]*Gain[J];
 const float Alpha=1.f-FMath::Exp(-2.f*PI*2600.f/Rate);float Low=0,Peak=.0001f;for(float& V:Wet){Low+=(V-Low)*Alpha;V=Low;Peak=FMath::Max(Peak,FMath::Abs(V));}
 TArray<int16> Out;Out.SetNumUninitialized(Wet.Num());for(int32 I=0;I<Out.Num();++I)Out[I]=int16(FMath::Clamp(Wet[I]/Peak*.85f,-.9f,.9f)*32767.f);return Out;
}''']
(HERE/'recorded_snare_candidate.inc').write_text('\n'.join(lines)+'\n');source['prepared_pcm']=entries;source['scope']='Six CC0 real acoustic snare hits, prepared as450ms/16kHz/mono16-bit PCM, embedded in the owned Mood.cpp candidate. No Content import/new dependency. Not yet compiled or accepted.';(HERE/'SOURCE.json').write_text(json.dumps(source,indent=2)+'\n')
print('Prepared six450ms recorded hits; candidate source bytes:',(HERE/'recorded_snare_candidate.inc').stat().st_size)

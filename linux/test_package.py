"""Check the real native/cross package and keep its architecture-specific name."""
from pathlib import Path
import os,shutil,subprocess,sys,tempfile,zipfile
binary=Path(sys.argv[1]).resolve()
options=sys.argv[2:];extra=6 if '--dsperate' in options else 0
# Stand-in cores: the binary's own bytes (the package checks a core's architecture, not what it does).
cores=Path(tempfile.mkdtemp(prefix='pd-cores-'))
for c in ('pokedaisy_gba_libretro.so','pokedaisy_gbc_libretro.so'):(cores/c).write_bytes(binary.read_bytes())
options+=['--core',str(cores/'pokedaisy_gba_libretro.so'),'--core',str(cores/'pokedaisy_gbc_libretro.so')]
extra+=4 # the two cores, armGBA's and zlib's licenses
arch=int.from_bytes(binary.read_bytes()[18:20],'little')
name='pokedaisy-linux-'+{62:'x86_64',183:'aarch64'}[arch]
if '--dsperate' in options:name+='-ds-experiment'
script=Path(__file__).with_name('package.py')
with tempfile.TemporaryDirectory(prefix='pd-linux-package-') as d:
    result=subprocess.run([sys.executable,str(script),*options,str(binary),d],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    archive=Path(d)/(name+'.zip')
    assert archive.exists() and Path(result.stdout.strip())==archive
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        names=z.namelist();assert len(names)==12+extra,names
        if '--dsperate' in options:
            assert name+'/DS-EXPERIMENT.md' in names
            assert name+'/licenses/FreeBIOS-BSD.txt' in names
            assert b'DSperate:' in z.read(name+'/SOURCE.txt')
        assert all(n.startswith(name+'/') for n in names)
        assert b'"label": "PokeDaisy"' in z.read(name+'/config.json') and z.read(name+'/icon.png')[:4]==b'\x89PNG'
        assert z.read(name+'/pokedaisy-linux')==binary.read_bytes()
        assert name+'/pokedaisy_gba_libretro.so' in names and name+'/licenses/armGBA-MPL-2.0.txt' in names
        assert b'armGBA: ' in z.read(name+'/SOURCE.txt')
        assert z.getinfo(name+'/launch.sh').external_attr>>16 & 0o111
        assert z.read(name+'/SOURCE.txt').startswith(b'Source: https://github.com/lidor30/pokedaisy')
        assert not any(n.lower().endswith(('.gb','.gbc','.gba','.sav','.ss','.srm','.bin')) for n in names)
    before=archive.read_bytes()
    result=subprocess.run([sys.executable,str(script),*options,str(binary),d],capture_output=True,text=True)
    assert result.returncode!=0 and archive.read_bytes()==before
# The PortMaster layout: PokeDaisy.sh beside pokedaisy/, which holds the rest.
pm='pokedaisy-portmaster-'+{62:'x86_64',183:'aarch64'}[arch]
if '--dsperate' in options:pm+='-ds-experiment'
with tempfile.TemporaryDirectory(prefix='pd-linux-package-') as d:
    result=subprocess.run([sys.executable,str(script),'--portmaster',*options,str(binary),d],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    with zipfile.ZipFile(Path(d)/(pm+'.zip')) as z:
        names=z.namelist();assert len(names)==10+extra,names
        assert z.getinfo(pm+'/PokeDaisy.sh').external_attr>>16 & 0o111
        assert z.read(pm+'/pokedaisy/pokedaisy-linux')==binary.read_bytes()
        assert pm+'/pokedaisy/pokedaisy_gbc_libretro.so' in names
        assert z.getinfo(pm+'/pokedaisy/pokedaisy-linux').external_attr>>16 & 0o111
        assert not any(n.endswith('launch.sh') for n in names)
        assert all(n==pm+'/PokeDaisy.sh' or n.startswith(pm+'/pokedaisy/') for n in names)
# AppImage AppDir layout + stand-in appimagetool invocation.
with tempfile.TemporaryDirectory(prefix='pd-linux-appdir-') as d:
    d_path=Path(d)
    result=subprocess.run([sys.executable,str(script),'--appdir-only',*options,str(binary),str(d_path)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    appdir=d_path/'PokeDaisy.AppDir'
    assert appdir.is_dir() and Path(result.stdout.strip())==appdir
    assert (appdir/'usr/bin/pokedaisy-linux').read_bytes()==binary.read_bytes()
    assert (appdir/'usr/bin/pokedaisy-linux').stat().st_mode & 0o111
    assert (appdir/'usr/bin/pokedaisy_gba_libretro.so').read_bytes()==binary.read_bytes()
    assert (appdir/'usr/bin/pokedaisy_gbc_libretro.so').exists()
    assert (appdir/'AppRun').stat().st_mode & 0o111
    desktop=(appdir/'PokeDaisy.desktop').read_text()
    assert 'Categories=Game;Emulator;' in desktop and 'Exec=pokedaisy-linux' in desktop
    assert (appdir/'pokedaisy.png').read_bytes()[:4]==b'\x89PNG'
    assert (appdir/'.DirIcon').read_bytes()[:4]==b'\x89PNG'
    assert (appdir/'LICENSE').exists() and (appdir/'NOTICE').exists()
    assert (appdir/'licenses/mGBA-MPL-2.0.txt').exists()
    assert (appdir/'licenses/armGBA-MPL-2.0.txt').exists()
    for grid_name in ('grid-vertical.png','grid-horizontal.png','hero.png','logo.png'):
        assert (appdir/'steam-grid'/grid_name).read_bytes()[:4]==b'\x89PNG'
    if shutil.which('ldd') and 'libSDL2-2.0.so' in subprocess.run(['ldd',str(binary)],capture_output=True,text=True).stdout:
        assert list((appdir/'usr/lib').iterdir())==[appdir/'usr/lib/libSDL2-2.0.so.0']
    # Overwrite refusal
    assert subprocess.run([sys.executable,str(script),'--appdir-only',*options,str(binary),str(d_path)],capture_output=True,text=True).returncode!=0

with tempfile.TemporaryDirectory(prefix='pd-linux-appimage-') as d:
    d_path=Path(d)
    fake_sdl=d_path/'libSDL2-2.0.so.0';fake_sdl.write_bytes(b'SDL2')
    fake_tool=d_path/'fake-appimagetool';fake_tool.write_text('#!/bin/sh\necho "$APPIMAGE_EXTRACT_AND_RUN:$ARCH:$*" > "$3"\n');fake_tool.chmod(0o755)
    env=dict(os.environ,APPIMAGETOOL=str(fake_tool))
    result=subprocess.run([sys.executable,str(script),'--appimage','--sdl2-lib',str(fake_sdl),*options,str(binary),str(d_path)],capture_output=True,text=True,env=env)
    assert result.returncode==0,result.stderr
    appimage=d_path/(name+'.AppImage')
    assert appimage.exists() and Path(result.stdout.strip())==appimage
    expected_arch={62:'x86_64',183:'aarch64'}[arch]
    assert '--no-appstream' in appimage.read_text() and f'1:{expected_arch}:' in appimage.read_text()
print('PASS architecture-specific package, actual executable, license/source records, overwrite refusal, PortMaster, and AppImage layout')

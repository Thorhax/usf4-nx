<p align="center"><img src="app/icon.jpg" width="128" alt="usf4-nx icon"></p>

# usf4-nx

A static recompilation of **Ultra Street Fighter IV** (Xbox 360, title 434307FC) for PC and Nintendo Switch, made with
the ReXGlue SDK. It follows the method of
[sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) (`docs/porting-a-360-game-prompt.md`).

**This repository holds no game data.** You need your own copy of the game: the Xbox 360 disc, dumped to a plain
`.iso`. The game files and the shaders made from them are never committed or published; the installer makes them from
your own disc.

## Status

- **PC (Windows):** boots, menus and fights work with the native Vulkan renderer at 60 fps.
- **Switch (Atmosphère, homebrew NRO):** since 0.8.0 the game draws with Renut's renderer on deko3d, the Switch's own
  graphics API (reNut by masterspike52, used with the author's permission, as banjo-nx and sa2-nx carry it), at 1280x720 in
  handheld and docked mode. Fights hold 60 fps with the CPU at 1785 MHz and the GPU at 768 MHz (Training and the
  attract-mode fights, measured on a console in October 2026). The menus and the controller settings screen show Switch buttons
  (`usf4_button_icons` in `usf4.toml`: `"switch"` or `"xbox"`). Video clip recording is turned off.

Version history, measurements and every finding: [docs/usf4-notes.md](docs/usf4-notes.md).
How the Switch renderer reached 60 fps on deko3d, for other ports: [docs/deko3d-renderer-usf4.md](docs/deko3d-renderer-usf4.md).

## How to install (Switch)

1. Open the installer page: **https://thorhax.github.io/usf4-nx-installer/**
2. Choose your game's disc image (a plain `.iso` of the Xbox 360 disc, about 8.7 GB, not a compressed CCI/GOD/ZAR
   image) or a folder you extracted the disc into, then press **Create usf4-nx.zip**. Everything runs in your browser
   and your game files never leave your computer: the page checks your copy, downloads the Switch build from this
   repository's releases, and makes the shaders and the game folder from your files.
3. Extract the downloaded `usf4-nx.zip` (about 8 GB) into `sdmc:/switch/`.

Your `usf4-nx` folder then holds `usf4.nro`, `LICENSES.txt`, the renderer's shaders (`deko3d_glsl.bin`, made from your
disc) and the game files in `game_root/`. Start `usf4.nro` with a title takeover (hold **R** while launching an installed game) or a
forwarder with full memory; the album applet does not give the game enough memory. The game writes `usf4.toml` (your
settings) at its first start.

**Updating:** open the installer page again and press **Create usf4-nx-update.zip**: it only holds the new NRO and the
shaders. Extract it over your folder; your settings and game files stay. From 0.7.x, `sa2_shaders.mesp` and
`sa2_shaders.mesp.idx` are no longer used and can be deleted. If your `usf4.toml` sets the render height to 540, 0.8.0
draws at 720 anyway (its renderer has no 540p). From 0.8.0 to 0.8.1 only `usf4.nro` changes.

**The first time you play,** new places can stop for a moment while their shaders are compiled (the first fight's
loading screen can take several seconds longer); they are kept on the SD card (`deko3d_shaders.bin`) for the next
sessions. Fights hold 60 fps with the CPU at 1785 MHz and the GPU at 768 MHz (set with sys-clk).

## Building

See [docs/building.md](docs/building.md): Windows (PC), Linux, and the Switch build in the `devkitpro/devkita64`
Docker image.

## Icon

`app/icon.jpg` is an original icon made by [tools/make_icon.py](tools/make_icon.py). To use your own on your Switch
build, put a 256x256 JPEG at `app/icon_local.jpg` (git-ignored) and rebuild.

## License

GPL-3.0, see [LICENSE](LICENSE). Third-party components: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Ultra
Street Fighter IV is a trademark of Capcom; this project is not affiliated with or endorsed by Capcom.

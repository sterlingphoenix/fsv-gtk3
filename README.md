## FSV

NOTE: this project **is** actively maintained! However, it is a stepping stone to a complete modernisation. 

This is the fork that updates from gtk2 to gtk3. This enables building on a modern Linux system (test case was Ubuntu 25.10).


This repo is a fork of [fsv](https://github.com/mcuelenaere/fsv), which is itself a fork of [fsv](http://fsv.sourceforge.net/).

The original author is [Daniel Richard G.](http://fox.mit.edu/skunk/), a former student of Computer Science at the MIT.

The version I forked form includes patches by (apparently) Maurus Cuelenaere. 

I did use generative ai to help me get this done in a timely manner. Also I was curious. 

**About fsv**

> fsv (pronounced eff-ess-vee) is a 3D file system visualizer. It lays out files and directories in three dimensions, geometrically representing the file system hierarchy to allow visual overview and analysis. fsv can visualize a modest home directory, a workstation's hard drive, or any arbitrarily large collection of files, limited only by the host computer's memory and graphics hardware. Note that this was originally written in the 1990s when 9.1GB harddrvies were considered "large". It still works, but some of the visualisations can be hard to grasp. 

Its ancestor, SGI's `fsn` (pronounced "fusion") originated on IRIX and was prominently featured in Jurassic Park. 

~Useful info and screenshots of the original SGI IRIX implementation are available on [siliconbunny](http://www.siliconbunny.com/fsn-the-irix-3d-file-system-tool-from-jurassic-park/).~ Apparently not.

**Requirements**

- GTK 3.16 or later (for GtkGLArea)
- libepoxy (for OpenGL function loading)
- OpenGL compatibility profile (legacy fixed-function pipeline)

The app sets `GDK_GL=legacy` automatically to request a GL compatibility profile context.
This works on NVIDIA and most Mesa drivers. On Wayland with some Mesa configurations,
you may need to fall back to X11: `GDK_BACKEND=x11 fsv /path`.

**Install**

1. `sudo apt-get install libgtk-3-dev libepoxy-dev meson ninja-build`
2. Clone the repository
3. Build:
    - `meson setup builddir`
    - `ninja -C builddir`
    - `sudo ninja -C builddir install`


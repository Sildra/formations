## Complilation

```bash
export LDFLAGS="$LDFLAGS -L$HOME/.local/lib"
export CPPFLAGS="$CPPFLAGS -I$HOME/.local/include"
export PATH="$HOME/.local/bin:$PATH"
export LD_LIBRARY_PATH=$HOME/.local/lib:$LD_LIBRARY_PATH
```

| Archive | Site |
| --- | --- |
| cgdb | https://cgdb.github.io/ |
| flex | https://github.com/westes/flex/releases |
| ncurses | https://ftp.gnu.org/gnu/ncurses/?C=M;O=D |
| readline | https://ftp.gnu.org/gnu/readline/?C=M;O=D |
| texinfo | https://ftp.gnu.org/gnu/texinfo/?C=M;O=D |

Pour chaque tar, extraire et compiler la bibliothèque

```bash
for archive in (texinfo readline ncurses flex cgdb); do
    tar xvf $archive*.tar.gz
    (cd $archive* && ./configure --prefix=$HOME/.local && make install)
done
```

## Exécution

L'exécution de CGDB suit le même principe que GDB. Pour le layout, comme le mode TUI de GDB, le code est affiché au dessus et un terminal GDB est disponible en dessous.

![CGDB debugging (from cgdb.github.io)](https://cgdb.github.io/images/screenshot_debugging.png)


Les bindings sont similaires à vim pour la navigation et VisualStudio pour les raccourcis de debug :

* F5 : Run
* F6 : Continue
* F7 : Finish
* F8 : Next
* F10 : Step

La doc complète est disponible [ici](https://cgdb.github.io/docs/cgdb.html).
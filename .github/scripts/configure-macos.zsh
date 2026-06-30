#!/usr/bin/env zsh

builtin emulate -L zsh
setopt ERR_EXIT
setopt ERR_RETURN
setopt NO_UNSET
setopt PIPE_FAIL

project_root="${0:A:h:h:h}"
cd "${project_root}"

if ! command -v conan >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then
    brew install conan
  else
    python3 -m pip install --user --upgrade conan
    export PATH="${HOME}/.local/bin:${PATH}"
  fi
fi

conan profile detect --force

qt_dir="/usr/local"
if command -v brew >/dev/null 2>&1; then
  qt_dir="$(brew --prefix qt@6 2>/dev/null || brew --prefix qt 2>/dev/null || echo '/usr/local')"
fi

cat > .env <<EOF
DISABLE_DEBUG=true
BUILD_TESTS=false
QT_DIR=${qt_dir}
SETUP_PORTABLE_OBS=false
EOF

python3 configure.py

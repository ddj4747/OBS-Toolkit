#!/usr/bin/env zsh

builtin emulate -L zsh
setopt ERR_EXIT
setopt ERR_RETURN
setopt NO_UNSET
setopt PIPE_FAIL

project_root="${0:A:h:h:h}"
cd "${project_root}"

if ! command -v conan >/dev/null 2>&1; then
  python3 -m pip install --user --upgrade conan
  export PATH="${HOME}/.local/bin:${PATH}"
fi

conan profile detect --force

cat > .env <<'EOF'
DISABLE_DEBUG=true
BUILD_TESTS=false
QT_DIR=/usr
EOF

python3 configure.py

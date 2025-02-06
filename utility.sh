argc=$#

show_usage() {
    echo "usage: ./utility.sh [help | build]\n\tnote: build <preset> <gen-clangd-config>(1 or 0)";
    exit $1
}

if [[ $argc -lt 1 || $argc -gt 3 ]]; then
    show_usage 1
fi

choice="$1"

if [[ "$choice" = "help" ]]; then
    show_usage 0
elif [[ "$choice" = "build" && $argc -eq 3 ]]; then
    cmake -S . -B build --preset $2 && cmake --build build;

    if [[ $3 -eq 1 ]]; then
        mv ./build/compile_commands.json .
    fi
else
    show_usage 1
fi

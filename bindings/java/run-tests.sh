#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
JAVAC="${GENLANG_JAVAC:-javac}"
JAVA="${GENLANG_JAVA:-java}"
JNA="${GENLANG_JNA_JAR:-}"
OUT="${GENLANG_JAVA_CLASSES:-${ROOT}/build/java-classes}"
LIBDIR="${GENLANG_LIB_DIR:-${ROOT}/build}"

if [[ -z "${JNA}" ]]; then
    if [[ -f "${LIBDIR}/jna/jna.jar" ]]; then
        JNA="${LIBDIR}/jna/jna.jar"
    elif [[ -f "${ROOT}/third_party/jna/jna.jar" ]]; then
        JNA="${ROOT}/third_party/jna/jna.jar"
    else
        echo "GENLANG_JNA_JAR is not set and jna.jar was not found" >&2
        exit 1
    fi
fi

mkdir -p "${OUT}"
"${JAVAC}" -encoding UTF-8 -cp "${JNA}" -d "${OUT}" \
    "${ROOT}"/bindings/java/src/genlang/*.java \
    "${ROOT}"/bindings/java/src/TestGenLang.java

export LD_LIBRARY_PATH="${LIBDIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
exec "${JAVA}" \
    -cp "${OUT}:${JNA}" \
    -Djna.library.path="${LIBDIR}" \
    -Djava.library.path="${LIBDIR}" \
    genlang.TestGenLang

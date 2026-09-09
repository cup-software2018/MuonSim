#!/bin/bash
#
# One cosmic muon run, configured by editing the block below. The job id names the
# output and the log, and picks the seed when SEED_BASE is set.
#
#   run/run_cosmic.sh 7
#   sbatch --array=1-64 run/run_cosmic.sh '$SLURM_ARRAY_TASK_ID'

set -euo pipefail

die() { echo "run_cosmic.sh: $*" >&2; exit 1; }

JID="${1:-}"
[[ -n "${JID}" ]] || die "needs a job id: run_cosmic.sh <jobid>"
[[ "${JID}" =~ ^[0-9]+$ ]] || die "job id must be a non-negative integer, is '${JID}'"
if (( $# > 1 )); then
  shift
  die "unexpected argument '$1' -- the job id is the only one"
fi

# --- What to run: edit these ----------------------------------------------

PROJECT_ROOT="/home/cupsoft/Works/AMoRE/Muons/MuonSim"

EXECUTABLE="${PROJECT_ROOT}/install/bin/MuonSim"

MACRO="${PROJECT_ROOT}/macro/cosmic_muon.mac"
OUTDIR="${PROJECT_ROOT}/run/output"
TAG="cosmic"

# Version list under ${PRODUCTS_DIR}/vlist; 3.0 is ROOT 6.32.20, GEANT4 11.1.3.
PROD_SETUP="/home/cupsoft/setup_prod.sh"
PROD_VERSION="3.0"

# Must match EXECUTABLE: install/lib64 (lib64, not lib) or PROJECT_ROOT/build.
MUONSIM_LIBDIR="${PROJECT_ROOT}/install/lib64"

NEVENT=50000

# 0 = from the clock. A base makes the seed SEED_BASE + jobid, so array tasks differ
# AND are reproducible; one fixed seed has them all simulate identical events.
SEED_BASE=0

# --------------------------------------------------------------------------

if (( SEED_BASE > 0 )); then
  SEED=$(( SEED_BASE + JID ))
else
  SEED=0
fi

OUTPUT="${OUTDIR}/${TAG}_${JID}.root"
LOG="${OUTDIR}/${TAG}_${JID}.log"

[[ -r "${PROD_SETUP}" ]] || die "no site setup script: ${PROD_SETUP}"

# A batch system starts jobs with no HOME, and geant4make.sh dereferences it bare.
if [[ -z "${HOME:-}" ]]; then
  HOME="$(getent passwd "$(id -u)" 2>/dev/null | cut -d: -f6 || true)"
  [[ -n "${HOME}" ]] || die "HOME is unset and cannot be looked up for uid $(id -u)"
  export HOME
fi

# -u off across the site scripts: they are not written for it. Ours stays strict.
set +u
# shellcheck disable=SC1090
. "${PROD_SETUP}" "${PROD_VERSION}"
set -u

# setup_prod.sh reports a bad version list and RETURNS. ROOTSYS proves it worked.
[[ -n "${ROOTSYS:-}" ]] || die "${PROD_SETUP} ${PROD_VERSION} set no ROOTSYS -- is '${PROD_VERSION}' a version list?"

# ${VAR:+:${VAR}}, not a bare ${VAR}: an empty element means the current directory.
export LD_LIBRARY_PATH="${MUONSIM_LIBDIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# So a macro naming a data file by a relative path is found from any directory.
export MUONSIM_DATA="${PROJECT_ROOT}/data"

# Before the log is opened, so a failure reaches the scheduler's own output.
[[ -d "${PROJECT_ROOT}" ]] || die "PROJECT_ROOT does not exist: ${PROJECT_ROOT}"
[[ -x "${EXECUTABLE}" ]]   || die "not an executable: ${EXECUTABLE}"
[[ -r "${MACRO}" ]]        || die "cannot read macro: ${MACRO}"
[[ -d "${MUONSIM_DATA}" ]] || die "no data directory: ${MUONSIM_DATA}"
[[ -d "${MUONSIM_LIBDIR}" ]] || die "no library directory: ${MUONSIM_LIBDIR}"
[[ "${NEVENT}" =~ ^[0-9]+$ ]]    || die "NEVENT must be a non-negative integer, is '${NEVENT}'"
[[ "${SEED_BASE}" =~ ^[0-9]+$ ]] || die "SEED_BASE must be a non-negative integer, is '${SEED_BASE}'"
(( SEED <= 900000000 ))    || die "SEED_BASE + ${JID} = ${SEED} is past the largest seed CLHEP takes (900000000)"

# Present but unrunnable: a tree missing its libraries looks fine until it starts.
"${EXECUTABLE}" --help > /dev/null 2>&1 || die "${EXECUTABLE} will not run -- check the library path"

mkdir -p -- "${OUTDIR}"

args=(-o "${OUTPUT}" -n "${NEVENT}")
(( SEED > 0 )) && args+=(-s "${SEED}")
args+=("${MACRO}")

# One line to the scheduler, the rest to the log. Not tee'd: a tee outlives the exec.
echo "run_cosmic.sh: job ${JID} logging to ${LOG}"
exec > "${LOG}" 2>&1

echo "job     : ${JID}"
# With its build date: a fix never `cmake --install`ed is otherwise invisible here.
echo "MuonSim : ${EXECUTABLE}  (built $(date -r "${EXECUTABLE}" '+%Y-%m-%d %H:%M'))"
echo "macro   : ${MACRO}"
echo "data    : ${MUONSIM_DATA}"
echo "prod    : ${PROD_SETUP} ${PROD_VERSION}"
echo "ROOTSYS : ${ROOTSYS}"
echo "libs    : ${LD_LIBRARY_PATH}"
echo "output  : ${OUTPUT}"
echo "events  : ${NEVENT}"
(( SEED > 0 )) && echo "seed    : ${SEED}  (SEED_BASE ${SEED_BASE} + job ${JID})" \
               || echo "seed    : from the clock"
echo

# exec: the scheduler's signals and exit status go straight to Geant4.
exec "${EXECUTABLE}" "${args[@]}"

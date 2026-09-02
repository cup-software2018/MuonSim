#!/bin/bash
#
# Step one of the rock gamma calculation: one run of decays in the rock, recording
# what escapes into the hall. Everything else is configured by editing the block
# below.
#
#   run/run_rockgamma.sh <jobid> <isotope>
#
#   run/run_rockgamma.sh 1 40K
#   sbatch --array=1-32 run/run_rockgamma.sh '$SLURM_ARRAY_TASK_ID' 232Th
#
# Job id first, the same as run_cosmic.sh: it is what a scheduler substitutes, so
# the two scripts submit the same way. The isotope follows because it is the one
# thing that varies between submissions, and it appears here once, so the output
# name and what was simulated cannot disagree. It reaches the macro through the
# environment.

set -euo pipefail

die() { echo "run_rockgamma.sh: $*" >&2; exit 1; }

JID="${1:-}"
ISOTOPE="${2:-}"
[[ -n "${JID}" && -n "${ISOTOPE}" ]] || die "usage: run_rockgamma.sh <jobid> <isotope>"

[[ "${JID}" =~ ^[0-9]+$ ]] || die "job id must be a non-negative integer, is '${JID}'"
# Letters and digits only. This goes into the environment and into a filename, so a
# stray space or slash would either name the wrong nuclide or write somewhere else.
[[ "${ISOTOPE}" =~ ^[0-9]*[A-Za-z][A-Za-z0-9]*$ ]] \
  || die "isotope '${ISOTOPE}' is not a bare name like 40K, 238U, 232Th"
if (( $# > 2 )); then
  shift 2
  die "unexpected argument '$1' -- the job id and the isotope are the only two"
fi

# --- What to run: edit these ----------------------------------------------

PROJECT_ROOT="/home/cupsoft/Works/AMoRE/Muons/MuonSim"

EXECUTABLE="${PROJECT_ROOT}/install/bin/MuonSim"

MACRO="${PROJECT_ROOT}/macro/rockgamma_batch.mac"
OUTDIR="${PROJECT_ROOT}/run/output"

# Version list under ${PRODUCTS_DIR}/vlist; 3.0 is ROOT 6.32.20, GEANT4 11.1.3.
PROD_SETUP="/home/cupsoft/setup_prod.sh"
PROD_VERSION="3.0"

# Must match EXECUTABLE: install/lib64 (lib64, not lib) or PROJECT_ROOT/build.
MUONSIM_LIBDIR="${PROJECT_ROOT}/install/lib64"

# Decays this job simulates. Only about one in 10^3 leaves the rock, so this wants
# to be large: 10^6 gives a few thousand escaping gammas.
NEVENT=1000000

# 0 = from the clock. A base makes the seed SEED_BASE + jobid, so array tasks differ
# AND are reproducible; one fixed seed has them all simulate identical events.
SEED_BASE=0

# --------------------------------------------------------------------------

if (( SEED_BASE > 0 )); then
  SEED=$(( SEED_BASE + JID ))
else
  SEED=0
fi

[[ -r "${MACRO}" ]] || die "cannot read macro: ${MACRO}"

TAG="rock${ISOTOPE}"

# The observer is what makes this a rock gamma run: without it the job costs the
# same and produces no Escape tree at all, which is only visible afterwards.
grep -qE '^[[:space:]]*/observer/step/enable[[:space:]]+(rockgamma|all)' "${MACRO}" \
  || die "${MACRO} never enables the step/rockgamma observer -- the run would record no escapes"

OUTPUT="${OUTDIR}/${TAG}_${JID}.root"
LOG="${OUTDIR}/${TAG}_${JID}.log"

# How the macro learns the isotope, via /control/getEnv. A missing variable stops
# the macro before the beam, but MuonSim still exits 0, so the check that matters is
# the one above -- by here the name is already known to be a bare token.
export ROCKGAMMA_ISOTOPE="${ISOTOPE}"

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
echo "run_rockgamma.sh: job ${JID}, ${ISOTOPE}, logging to ${LOG}"
exec > "${LOG}" 2>&1

echo "job     : ${JID}"
# With its build date: a fix never `cmake --install`ed is otherwise invisible here.
echo "MuonSim : ${EXECUTABLE}  (built $(date -r "${EXECUTABLE}" '+%Y-%m-%d %H:%M'))"
echo "macro   : ${MACRO}"
echo "isotope : ${ISOTOPE}  (ROCKGAMMA_ISOTOPE)"
echo "data    : ${MUONSIM_DATA}"
echo "prod    : ${PROD_SETUP} ${PROD_VERSION}"
echo "ROOTSYS : ${ROOTSYS}"
echo "libs    : ${LD_LIBRARY_PATH}"
echo "output  : ${OUTPUT}"
echo "decays  : ${NEVENT}"
(( SEED > 0 )) && echo "seed    : ${SEED}  (SEED_BASE ${SEED_BASE} + job ${JID})" \
               || echo "seed    : from the clock"
echo

# exec: the scheduler's signals and exit status go straight to Geant4.
exec "${EXECUTABLE}" "${args[@]}"

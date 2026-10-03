#include <KillOnCloseJob.h>

#include <plugin-support.h>

#include <QProcess>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
namespace {
bool currentProcessIsInJob() {
	BOOL inJob = FALSE;
	if (!IsProcessInJob(GetCurrentProcess(), nullptr, &inJob)) {
		obs_log(LOG_WARNING, "IsProcessInJob failed (%lu)", GetLastError());
		return false;
	}
	return inJob == TRUE;
}

DWORD currentJobLimitFlags() {
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
	if (!QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation, &info, sizeof(info), nullptr)) {
		return 0;
	}
	return info.BasicLimitInformation.LimitFlags;
}

HANDLE killOnCloseJob() {
	static const HANDLE job = []() -> HANDLE {
		if (currentProcessIsInJob()) {
			const DWORD flags = currentJobLimitFlags();
			const bool canBreakAway = (flags & JOB_OBJECT_LIMIT_BREAKAWAY_OK) != 0 ||
						  (flags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK) != 0;
			if (!canBreakAway) {
				obs_log(LOG_INFO,
					"OBS is in a job that disallows breakaway; skip kill-on-close job");
				return nullptr;
			}
		}

		const HANDLE handle = CreateJobObjectW(nullptr, nullptr);
		if (handle == nullptr) {
			obs_log(LOG_ERROR, "CreateJobObjectW failed (%lu)", GetLastError());
			return nullptr;
		}

		(void)SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);

		JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
		info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
			obs_log(LOG_ERROR, "SetInformationJobObject failed (%lu)", GetLastError());
			CloseHandle(handle);
			return nullptr;
		}

		return handle;
	}();
	return job;
}
} // namespace
#endif

void attachToKillOnCloseJob(QProcess *process) {
	if (process == nullptr) {
		return;
	}

#ifdef _WIN32
	if (currentProcessIsInJob()) {
		const DWORD flags = currentJobLimitFlags();
		if ((flags & JOB_OBJECT_LIMIT_BREAKAWAY_OK) != 0 &&
		    (flags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK) == 0) {
			process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
				args->flags |= CREATE_BREAKAWAY_FROM_JOB;
			});
		}
	}

	QObject::connect(process, &QProcess::started, process, [process]() {
		const HANDLE job = killOnCloseJob();
		if (job == nullptr) {
			return;
		}

		const HANDLE child =
			OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(process->processId()));
		if (child == nullptr) {
			obs_log(LOG_ERROR, "OpenProcess failed (%lu)", GetLastError());
			return;
		}

		if (!AssignProcessToJobObject(job, child)) {
			obs_log(LOG_ERROR, "AssignProcessToJobObject failed (%lu)", GetLastError());
		}
		CloseHandle(child);
	});
#elif defined(Q_OS_LINUX)
	process->setChildProcessModifier([]() {
		prctl(PR_SET_PDEATHSIG, SIGKILL);
		if (getppid() == 1) {
			_exit(1);
		}
	});
#else
	Q_UNUSED(process);
#endif
}

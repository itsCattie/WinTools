using System.Diagnostics;
using AppLogger = Logger.Logger;
using LogSeverity = Logger.Servarity;

// MediaBar: MediaBarForm manages feature behavior.

namespace Modules.MediaBar;

internal sealed class MediaBarForm : Form
{
	private const string LogSource = "MediaBarLauncher";
	private readonly Label _statusLabel;
	private readonly Button _launchButton;
	private readonly Button _buildButton;
	private readonly string _executableName = "MediaBarCpp.exe";

	public MediaBarForm()
	{
		Text = "MediaBar Launcher";
		StartPosition = FormStartPosition.CenterParent;
		FormBorderStyle = FormBorderStyle.FixedDialog;
		MaximizeBox = false;
		MinimizeBox = false;
		ClientSize = new Size(460, 165);

		_statusLabel = new Label
		{
			AutoSize = false,
			Location = new Point(14, 14),
			Size = new Size(430, 70),
			Text = "Launching MediaBar...",
			TextAlign = ContentAlignment.MiddleLeft
		};

		_launchButton = new Button
		{
			Text = "Launch MediaBar",
			Location = new Point(14, 102),
			Size = new Size(138, 32)
		};
		_launchButton.Click += (_, _) => TryLaunchAndCloseOnSuccess();

		_buildButton = new Button
		{
			Text = "Build MediaBar",
			Location = new Point(164, 102),
			Size = new Size(138, 32)
		};
		_buildButton.Click += (_, _) => RunBuildScript();

		Controls.Add(_statusLabel);
		Controls.Add(_launchButton);
		Controls.Add(_buildButton);

		AppLogger.Log(LogSource, LogSeverity.Pass, "Launcher initialized.");
		Shown += (_, _) => TryLaunchAndCloseOnSuccess();
	}

	private void TryLaunchAndCloseOnSuccess()
	{
		var executablePath = FindMediaBarExecutable();
		if (string.IsNullOrWhiteSpace(executablePath))
		{
			_statusLabel.Text = "MediaBar executable not found. Build it with scripts\\build-mediabar.ps1, then click Launch again.";
			AppLogger.Log(LogSource, LogSeverity.Warning, "Executable not found.");
			return;
		}

		try
		{
			var startInfo = new ProcessStartInfo
			{
				FileName = executablePath,
				WorkingDirectory = Path.GetDirectoryName(executablePath) ?? AppContext.BaseDirectory,
				UseShellExecute = false,
				CreateNoWindow = true
			};

			_ = Process.Start(startInfo);
			AppLogger.Log(LogSource, LogSeverity.Pass, "Launched MediaBar executable.", executablePath);
			Close();
		}
		catch (Exception ex)
		{
			_statusLabel.Text = $"Failed to launch MediaBar: {ex.Message}";
			AppLogger.Log(LogSource, LogSeverity.Error, "Failed to launch MediaBar executable.", ex.Message);
		}
	}

	private string? FindMediaBarExecutable()
	{
		var repoRoot = FindRepositoryRoot();
		var candidates = new List<string>
		{
			Path.Combine(repoRoot, "src", "modules", "MediaBar", "build", "Release", _executableName),
			Path.Combine(repoRoot, "src", "modules", "MediaBar", "build", _executableName),
			Path.Combine(AppContext.BaseDirectory, "modules", "MediaBar", _executableName),
			Path.Combine(AppContext.BaseDirectory, _executableName)
		};

		foreach (var candidate in candidates)
		{
			if (IsDeploymentReady(candidate))
			{
				AppLogger.Log(LogSource, LogSeverity.Pass, "Using deployment-ready executable.", candidate);
				return candidate;
			}
		}

		AppLogger.Log(LogSource, LogSeverity.Warning, "No deployment-ready executable found.");
		return null;
	}

	private void RunBuildScript()
	{
		var repoRoot = FindRepositoryRoot();
		var scriptPath = Path.Combine(repoRoot, "scripts", "build-mediabar.ps1");
		if (!File.Exists(scriptPath))
		{
			_statusLabel.Text = "Build script not found at scripts\\build-mediabar.ps1.";
			AppLogger.Log(LogSource, LogSeverity.Error, "Build script not found.", scriptPath);
			return;
		}

		try
		{
			var startInfo = new ProcessStartInfo
			{
				FileName = "powershell.exe",
				Arguments = $"-NoProfile -ExecutionPolicy Bypass -File \"{scriptPath}\"",
				UseShellExecute = true
			};

			_ = Process.Start(startInfo);
			_statusLabel.Text = "Build script started in a new PowerShell window.";
			AppLogger.Log(LogSource, LogSeverity.Pass, "Build script started.", scriptPath);
		}
		catch (Exception ex)
		{
			_statusLabel.Text = $"Failed to start build script: {ex.Message}";
			AppLogger.Log(LogSource, LogSeverity.Error, "Failed to start build script.", ex.Message);
		}
	}

	private static string FindRepositoryRoot()
	{
		var current = new DirectoryInfo(AppContext.BaseDirectory);
		while (current is not null)
		{
			if (File.Exists(Path.Combine(current.FullName, "WinTools.Master.csproj")))
			{
				return current.FullName;
			}

			current = current.Parent;
		}

		return AppContext.BaseDirectory;
	}

	private static bool IsDeploymentReady(string executablePath)
	{
		if (!File.Exists(executablePath))
		{
			return false;
		}

		var directory = Path.GetDirectoryName(executablePath);
		if (string.IsNullOrWhiteSpace(directory))
		{
			return false;
		}

		var qtCoreDll = Path.Combine(directory, "Qt6Core.dll");
		var qtPlatformPlugin = Path.Combine(directory, "platforms", "qwindows.dll");
		return File.Exists(qtCoreDll) && File.Exists(qtPlatformPlugin);
	}
}

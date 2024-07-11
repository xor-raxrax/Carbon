using System;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows.Input;
using System.Security.Cryptography;
using Microsoft.Win32;
using Microsoft.Web.WebView2.Wpf;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Windows;
using System.Windows.Controls;
using System.Text.RegularExpressions;

namespace CarbonGui
{

	public class MainFileChecker
	{
		public MainFileChecker(MainWindow window, ListBox listBox)
		{
			scriptFiles = new ObservableCollection<string>();
			listBox.ItemsSource = scriptFiles;
			listBox.SelectionChanged += window.OnScriptListBoxSelectionChanged;
			this.window = window;
			InitializeFileWatcher();
			LoadExistingFiles();
		}

		public void CheckDll()
		{
			if (!File.Exists(PathConfig.Dll))
				RaiseException("Missing DLL at path: " + Path.GetFullPath(PathConfig.Dll));
		}

		public void CheckDump()
		{
			if (File.Exists(PathConfig.Dump))
				return;

			Console.WriteLine("Missing dump file, creating new one");

			if (!File.Exists(PathConfig.AddressDumper))
				RaiseException("Missing dumper at path: ", Path.GetFullPath(PathConfig.AddressDumper));

			var processInfo = new ProcessStartInfo(PathConfig.AddressDumper, PathConfig.Dump)
			{
				UseShellExecute = false
			};

			using (var process = Process.Start(processInfo))
			{
				process.WaitForExit();
				if (process.ExitCode != 0)
					RaiseException("AddressDumper failed");
			}
		}

		public void CheckUserDirectory()
		{
			if (!Directory.Exists(PathConfig.UserDirectory))
				Directory.CreateDirectory(PathConfig.UserDirectory);
		}

		public void CheckScriptDirectory()
		{
			if (!Directory.Exists(PathConfig.Scripts))
				Directory.CreateDirectory(PathConfig.Scripts);
		}

		private void RaiseException(string message, string path = null)
		{
			throw new FileNotFoundException($"{message}: {path}");
		}

		private void InitializeFileWatcher()
		{
			CheckScriptDirectory();

			fileWatcher = new FileSystemWatcher
			{
				Path = PathConfig.Scripts,
				NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite,
				Filter = "*.*"
			};

			fileWatcher.Created += OnFileChanged;
			fileWatcher.Deleted += OnFileChanged;
			fileWatcher.EnableRaisingEvents = true;
		}

		private void LoadExistingFiles()
		{
			scriptFiles.Clear();
			foreach (var file in Directory.GetFiles(PathConfig.Scripts))
			{
				scriptFiles.Add(Path.GetFileName(file));
			}
		}

		private void OnFileChanged(object sender, FileSystemEventArgs e)
		{
			Application.Current.Dispatcher.Invoke(() =>
			{
				switch (e.ChangeType)
				{
					case WatcherChangeTypes.Created:
						scriptFiles.Add(Path.GetFileName(e.FullPath));
						break;
					case WatcherChangeTypes.Deleted:
						scriptFiles.Remove(Path.GetFileName(e.FullPath));
						break;
				}
			});
		}

		private FileSystemWatcher fileWatcher;
		private ObservableCollection<string> scriptFiles;
		private MainWindow window;
	}

	class MonacoEditor
	{
		class MonacoFilesChecker
		{
			public void CheckMonacoFiles()
			{
				string basePath = Path.Combine(Directory.GetCurrentDirectory(), MonacoPath);
				CheckFile(basePath, "index.html", "CarbonGui.Resources.index.html");
				CheckFile(basePath, "vs/loader.js", "CarbonGui.Resources.vs.loader.js");
				CheckFile(basePath, "vs/base/worker/workerMain.js", "CarbonGui.Resources.vs.base.worker.workerMain.js");
				CheckFile(basePath, "vs/editor/editor.main.js", "CarbonGui.Resources.vs.editor.editor.main.js");
				CheckFile(basePath, "vs/editor/editor.main.css", "CarbonGui.Resources.vs.editor.editor.main.css");
				CheckFile(basePath, "vs/editor/editor.main.nls.js", "CarbonGui.Resources.vs.editor.editor.main.nls.js");
				CheckFile(basePath, "vs/basic-languages/lua/lua.js", "CarbonGui.Resources.vs.basiclanguages.lua.lua.js");
			}

			public string getIndexPath()
			{
				string basePath = Path.Combine(Directory.GetCurrentDirectory(), MonacoPath);
				string indexPath = Path.Combine(basePath, "index.html");
				return indexPath.Replace('\\', '/');
			}

			private void CheckFile(string basePath, string relativePath, string resourceName)
			{
				string filePath = Path.Combine(basePath, relativePath);
				bool fileExists = File.Exists(filePath);
				bool isDifferent = false;

				Stream resourceStream = Assembly.GetExecutingAssembly().GetManifestResourceStream(resourceName);
				if (resourceStream == null)
					throw new FileNotFoundException($"Resource {resourceName} not found.");

				if (fileExists && (CalculateFileHash(filePath) != CalculateStreamHash(resourceStream)))
					isDifferent = true;

				if (!fileExists || isDifferent)
				{
					string directory = Path.GetDirectoryName(filePath);
					if (!Directory.Exists(directory))
						Directory.CreateDirectory(directory);

					using (var fileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write))
						resourceStream.CopyTo(fileStream);
				}
			}

			private string CalculateFileHash(string filePath)
			{
				using (var md5 = MD5.Create())
				using (var fileStream = File.OpenRead(filePath))
					return BitConverter.ToString(md5.ComputeHash(fileStream));
			}

			private string CalculateStreamHash(Stream stream)
			{
				using (var md5 = MD5.Create())
				{
					var originalPosition = stream.Position;
					stream.Position = 0;
					string result = BitConverter.ToString(md5.ComputeHash(stream));
					stream.Position = originalPosition;
					return result;
				}
			}

			private const string MonacoPath = "bin/monaco";
		}

		public MonacoEditor(WebView2 codeEditor)
		{
			CodeEditor = codeEditor;
			var monacoFiles = new MonacoFilesChecker();
			monacoFiles.CheckMonacoFiles();
			CodeEditor.CoreWebView2.Navigate($"file:///{monacoFiles.getIndexPath()}");
		}
		
		public void SetEditorContent(string code)
		{
			CodeEditor.CoreWebView2.ExecuteScriptAsync($"getEditorContent({code})");
		}

		public async Task<string> GetEditorContentAsync()
		{
			var jsonString = await CodeEditor.CoreWebView2.ExecuteScriptAsync("getEditorContent()");

			if (jsonString.StartsWith("\"") && jsonString.EndsWith("\""))
				jsonString = jsonString.Substring(1, jsonString.Length - 2);

			return Regex.Unescape(jsonString);
		}

		WebView2 CodeEditor = null;
	}

	public partial class MainWindow : Window
	{
		public MainWindow()
		{
			InitializeComponent();
			CheckMainFiles();
			InitializeWebViewAsync();
		}
		private void CheckMainFiles()
		{
			mainFileChecker = new MainFileChecker(this, ScriptFilesListBox);
			mainFileChecker.CheckScriptDirectory();
			mainFileChecker.CheckUserDirectory();
			mainFileChecker.CheckDll();
			mainFileChecker.CheckDump();
		}

		private async void InitializeWebViewAsync()
		{
			Environment.SetEnvironmentVariable("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", "--disable-features=MetricsReportingEnabled,EnablePersistentHistograms");
			Environment.SetEnvironmentVariable("WEBVIEW2_USER_DATA_FOLDER", Path.GetTempPath());
			await CodeEditor.EnsureCoreWebView2Async(null);
			monacoEditor = new MonacoEditor(CodeEditor);
		}

		private void TopBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
		{
			if (e.ClickCount == 2)
			{
				if (WindowState == WindowState.Maximized)
					WindowState = WindowState.Normal;
				else
					WindowState = WindowState.Maximized;
			}
			else
			{
				DragMove();
			}
		}

		private void OnMinimizeButtonClicked(object sender, RoutedEventArgs e)
		{
			WindowState = WindowState.Minimized;
		}

		private void OnMaximizeButtonClicked(object sender, RoutedEventArgs e)
		{
			if (WindowState == WindowState.Maximized)
				WindowState = WindowState.Normal;
			else
				WindowState = WindowState.Maximized;
		}

		private void OnCloseButtonClicked(object sender, RoutedEventArgs e)
		{
			Close();
		}

		private async void OnExecuteButtonClicked(object sender, RoutedEventArgs e)
		{
			if (!checkCanRunCode())
				return;
			InjectionApi.SendScript( await monacoEditor.GetEditorContentAsync());
		}

		private void OnClearButtonClicked(object sender, RoutedEventArgs e)
		{
			monacoEditor.SetEditorContent("");
		}

		private void OnOpenFileButtonClicked(object sender, RoutedEventArgs e)
		{
			var openFileDialog = new OpenFileDialog
			{
				Filter = "Text files (*.txt)|*.txt|All files (*.*)|*.*"
			};

			if (openFileDialog.ShowDialog() == true)
				monacoEditor.SetEditorContent(File.ReadAllText(openFileDialog.FileName));
		}

		private async void OnSaveFileButtonClicked(object sender, RoutedEventArgs e)
		{
			var saveFileDialog = new SaveFileDialog
			{
				Filter = "Text files (*.txt)|*.txt|All files (*.*)|*.*"
			};

			if (saveFileDialog.ShowDialog() == true)
				File.WriteAllText(saveFileDialog.FileName, await monacoEditor.GetEditorContentAsync());
		}

		private void OnExecuteFileButtonClicked(object sender, RoutedEventArgs e)
		{
			if (!checkCanRunCode())
				return;

			var saveFileDialog = new OpenFileDialog
			{
				Filter = "Text files (*.txt)|*.txt|All files (*.*)|*.*"
			};

			if (saveFileDialog.ShowDialog() == true)
				InjectionApi.SendScript(File.ReadAllText(saveFileDialog.FileName));
		}

		private void OnSettingsButtonClicked(object sender, RoutedEventArgs e)
		{
			MessageBox.Show("no settings yet", ":woah:", MessageBoxButton.OK, MessageBoxImage.Asterisk);
		}

		private void OnAttachButtonClicked(object sender, RoutedEventArgs e)
		{
			if (InjectionApi.injectState == InjectionApi.InjectState.None)
			{
				var info = InjectionApi.TryInject();
				if (info)
				{
					MessageBox.Show(info.error, "failed to inject", MessageBoxButton.OK, MessageBoxImage.Error);
					return;
				}
			}
			else if (InjectionApi.injectState == InjectionApi.InjectState.Injected
				|| InjectionApi.injectState == InjectionApi.InjectState.ThreadGrabbed)
			{
				var addressWindow = new AddressWindow();
				addressWindow.ShowDialog();
			}
		}
		public void OnScriptListBoxSelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			if (e.AddedItems.Count > 0)
			{
				string selectedFile = e.AddedItems[0] as string;
				OnScriptFileClicked(Path.Combine(PathConfig.Scripts, selectedFile));
			}
		}
		
		public void OnScriptFileClicked(string filePath)
		{
			monacoEditor.SetEditorContent(File.ReadAllText(filePath));
		}

		private bool checkCanRunCode()
		{
			if (InjectionApi.injectState == InjectionApi.InjectState.None)
				MessageBox.Show("not attached", "cannot execute", MessageBoxButton.OK, MessageBoxImage.Error);
			else if (InjectionApi.injectState == InjectionApi.InjectState.Injected)
				MessageBox.Show("not attached to thread", "cannot execute", MessageBoxButton.OK, MessageBoxImage.Error);

			return InjectionApi.injectState == InjectionApi.InjectState.ThreadGrabbed;
		}

		MonacoEditor monacoEditor = null;
		MainFileChecker mainFileChecker = null;
	}
}

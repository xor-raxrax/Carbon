using System;
using System.Windows;
using System.Windows.Input;

namespace CarbonGui
{
	public partial class AddressWindow : Window
	{
		public AddressWindow()
		{
			InitializeComponent();
		}
		private void OnTopBarMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
		{
			if (e.ClickCount == 2)
			{
				if (WindowState == WindowState.Maximized)
					WindowState = WindowState.Normal;
				else
					WindowState = WindowState.Maximized;
			}
			else
				DragMove();
		}

		private void OnCloseButtonClicked(object sender, RoutedEventArgs e)
		{
			Close();
		}

		private void OnSubmitButtonClicked(object sender, RoutedEventArgs e)
		{
			if (InjectionApi.TryInjectEnvironment(AddressInput.Text))
			{
				MessageBox.Show("Please enter a valid hexadecimal address");
				return;
			}

			Close();
		}

		private void OnSnippetButtonClick(object sender, RoutedEventArgs e)
		{
            Clipboard.SetText("print({[coroutine.running()]=1})\nwhile not getreg do wait() end");
		}
	}
}

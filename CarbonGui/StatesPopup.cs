using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace CarbonGui
{
	public partial class StatesPopup
	{
		public void Initialize(TargetSettings targetSettings, MainWindow mainWindow, InjectionHandler injectionHandler)
		{
			this.mainWindow = mainWindow;
			this.injectionHandler = injectionHandler;
			this.targetSettings = targetSettings;
			popupStackPanel = mainWindow.LuaStatesPopupStackPanel;
			popup = mainWindow.LuaStatesPopup;
		}

		public void OnAvailableLuaStatesUpdated()
		{
			Application.Current.Dispatcher.Invoke(() => RenderStates());
		}

		void OnStateButtonClicked(object sender, RoutedEventArgs e)
		{
			popup.IsOpen = false;
			Button clickedButton = sender as Button;
			if (buttonToLuaState.TryGetValue(clickedButton, out LuaState luaState))
				targetSettings.SetLuaState(luaState);
		}

		public void OnTargetProcessChanged()
		{
			Application.Current.Dispatcher.Invoke(() => RenderStates());
		}

		private void RenderStates()
		{
			popupStackPanel.Children.Clear();
			groupExpanders.Clear();
			buttonToLuaState.Clear();

			foreach (var dataModel in injectionHandler.GetDataModels())
			{
				var foregroundTextBrush = new SolidColorBrush(Color.FromArgb(0xFF, 0xD3, 0xD3, 0xD3));
				var backgroundBrush = new SolidColorBrush(Color.FromArgb(0xFF, 0x2D, 0x2D, 0x2D));

				if (!groupExpanders.ContainsKey(dataModel.type))
				{
					Expander expander = new Expander
					{
						Header = dataModel.type.ToString(),
						Content = new StackPanel(),
						IsExpanded = true,
						Margin = new Thickness(0, 0, 0, 0),
						Foreground = foregroundTextBrush,
						Style = mainWindow.FindResource("Expander") as Style,
					};
					groupExpanders[dataModel.type] = expander;
					popupStackPanel.Children.Add(expander);
				}

				var expanderContent = groupExpanders[dataModel.type].Content as StackPanel;
				foreach (var state in dataModel.states)
				{
					StackPanel itemPanel = new StackPanel
					{
						Orientation = Orientation.Horizontal,
					};

					var margin = new Thickness(0, 5, 0, 0);

					// TODO: must be state type
					string imageName = DataModelTypeToImageName(dataModel.type);

					Image typeImage = new Image
					{
						Width = 15,
						Height = 15,
						Source = new BitmapImage(new Uri($"pack://application:,,,/Resources/{imageName}", UriKind.Absolute)),
						Margin = margin,
					};

					Button stateButton = new Button
					{
						Content = $"State {state.address.ToString("X")}",
						VerticalAlignment = VerticalAlignment.Center,
						Margin = margin,
						Background = backgroundBrush,
						Foreground = foregroundTextBrush,
						BorderThickness = new Thickness(0),
						Style = mainWindow.FindResource("HoverBottomButton") as Style,
					};

					buttonToLuaState[stateButton] = state;
					stateButton.Click += OnStateButtonClicked;

					itemPanel.Children.Add(typeImage);
					itemPanel.Children.Add(stateButton);
					expanderContent.Children.Add(itemPanel);
				}
			}
		}

		private string DataModelTypeToImageName(DataModelType type)
		{
			switch (type)
			{
				case DataModelType.Client:
					return "Client.png";
				case DataModelType.Server:
					return "Server.png";
			}

			return "Question.png";
		}

		private static Brush DataModelTypeToBrush(DataModelType type)
		{
			switch (type)
			{
				case DataModelType.Client:
					return Brushes.Blue;
				case DataModelType.Server:
					return Brushes.Green;
				case DataModelType.Unknown:
					return Brushes.Orange;
			}

			return Brushes.Gray;
		}


		private readonly Dictionary<Button, LuaState> buttonToLuaState = new Dictionary<Button, LuaState>();
		private readonly Dictionary<DataModelType, Expander> groupExpanders = new Dictionary<DataModelType, Expander>();

		private InjectionHandler injectionHandler = null;
		private MainWindow mainWindow = null;
		private StackPanel popupStackPanel = null;
		private Popup popup = null;
		private TargetSettings targetSettings = null;
	}
}
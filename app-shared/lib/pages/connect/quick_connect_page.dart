import 'package:flutter/material.dart';
import 'package:orchid/api/orchid_types.dart';
import 'package:orchid/pages/app_text.dart';
import 'package:orchid/pages/common/app_bar.dart';
import 'package:orchid/api/notifications.dart';
import 'package:orchid/pages/common/notification_banner.dart';
import 'package:orchid/pages/connect/connect_button.dart';
import 'package:orchid/api/orchid_api.dart';
import 'package:orchid/pages/common/side_drawer.dart';
import 'package:orchid/pages/common/options_bar.dart';
import 'package:orchid/pages/app_colors.dart';
import 'package:flare_flutter/flare_actor.dart';
import 'package:orchid/pages/connect/route_info.dart';
import 'package:orchid/pages/onboarding/onboarding.dart';
import 'package:rxdart/rxdart.dart';

class QuickConnectPage extends StatefulWidget {
  QuickConnectPage({Key key}) : super(key: key);

  @override
  _QuickConnectPageState createState() => _QuickConnectPageState();
}

class _QuickConnectPageState
    extends State<QuickConnectPage> // with SingleTickerProviderStateMixin {
    with
        TickerProviderStateMixin {
  // Current state reflected by the page, driving color and animation.
  OrchidConnectionState _connectionState = OrchidConnectionState.NotConnected;

  // Interpolates 0-1 on connection
  AnimationController _connectAnimController;

  static const qc_gradient_start = AppColors.grey_7;
  static const qc_gradient_end = AppColors.grey_6;
  static const qc_purple_gradient_start = AppColors.purple_2;
  static const qc_purple_gradient_end = AppColors.purple_1;

  Animation<Color> _gradientStart;
  Animation<Color> _gradientEnd;
  Animation<Color> _iconColor; // The options bar icons
  Animation<double> _animOpacity; // The background Flare animation

  // This determines whether the intro (slide in) or repeating background animation is shown.
  bool _showIntroAnimation = true;

  @override
  void initState() {
    super.initState();

    AppOnboarding().showPageIfNeeded(context);
    initListeners();
    initAnimations();
  }

  /// Listen for changes in Orchid network status.
  void initListeners() {
    // Monitor connection status
    OrchidAPI().connectionStatus.listen((OrchidConnectionState state) {
      _connectionStateChanged(state);
    });

    // Monitor sync status
    OrchidAPI().syncStatus.listen((OrchidSyncStatus value) {
      _syncStateChanged(value);
    });

    AppNotifications().notification.listen((_) {
      setState(() {}); // Trigger refresh of the UI
    });
  }

  /// Called upon a change to Orchid connection state
  void _connectionStateChanged(OrchidConnectionState state) {
    // Fade the background animation in or out based on which direction we are going.
    var fromConnected = _showConnectedBackground();
    var toConnected = _showConnectedBackgroundFor(state);

    if (toConnected && !fromConnected) {
      _connectAnimController.forward().then((_) {});
    }
    if (fromConnected && !toConnected) {
      _connectAnimController.reverse().then((_) {
        // Reset the animation sequence (intro then loop) for the next connect.
        setState(() {
          _showIntroAnimation = true;
        });
      });
    }

    setState(() {
      _connectionState = state;
    });
  }

  /// Called upon a change to Orchid sync state
  void _syncStateChanged(OrchidSyncStatus value) {
    setState(() {
      switch (value.state) {
        case OrchidSyncState.Complete:
          //_showSyncProgress = false;
          break;
        case OrchidSyncState.Required: // fall through
        case OrchidSyncState.InProgress:
        //_showSyncProgress = true;
      }
    });
  }

  /// True if we show the animated connected background for the given state.
  bool _showConnectedBackgroundFor(OrchidConnectionState state) {
    switch (state) {
      case OrchidConnectionState.NotConnected:
      case OrchidConnectionState.Connecting:
        return false;
      case OrchidConnectionState.Connected:
        return true;
    }
  }

  /// True if we show the animated connected background for the current state.
  bool _showConnectedBackground() {
    return _showConnectedBackgroundFor(_connectionState);
  }

  /// Set up animations
  void initAnimations() {
    _connectAnimController = AnimationController(
        duration: const Duration(milliseconds: 1000), vsync: this);

    _gradientStart =
        ColorTween(begin: qc_gradient_start, end: qc_purple_gradient_start)
            .animate(_connectAnimController);

    _gradientEnd =
        ColorTween(begin: qc_gradient_end, end: qc_purple_gradient_end)
            .animate(_connectAnimController);

    _iconColor = ColorTween(begin: AppColors.purple, end: AppColors.white)
        .animate(_connectAnimController);

    _animOpacity = Tween(begin: 0.0, end: 1.0) // same as controller for now
        .animate(_connectAnimController);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: SmallAppBar.build(context),
      body: buildPageContainer(context),
      drawer: SideDrawer(),
    );
  }

  // The page body holding bacground and options bar
  Widget buildPageContainer(BuildContext context) {
    String connectedAnimation = "assets/flare/Connection_screens.flr";
    String connectedAnimationIntroName = "connectedIntro";
    String connectedAnimationLoopName = "connectedLoop";
    String connectedAnimationName = _showIntroAnimation ? connectedAnimationIntroName : connectedAnimationLoopName;

    // Calculate the animation size and position
    double connectedAnimationAspectRatio = 360.0/340.0; // w/h
    double connectedAnimationPosition = 0.34; // vertical screen height fraction of center
    Size screenSize = MediaQuery.of(context).size;
    Size animationSize = Size(screenSize.width, screenSize.width / connectedAnimationAspectRatio);
    double animationTop = screenSize.height * connectedAnimationPosition - animationSize.height / 2;

    return Stack(
      children: <Widget>[
        // background gradient
        AnimatedBuilder(
          builder: (context, child) => Container(
                  decoration: BoxDecoration(
                gradient: LinearGradient(
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                    colors: [_gradientStart.value, _gradientEnd.value]),
              )),
          animation: _connectAnimController,
        ),

        // bottom map
        SafeArea(
          child: Container(
              margin: EdgeInsets.only(bottom: 15.0),
              decoration: BoxDecoration(
                  image: DecorationImage(
                      fit: BoxFit.fitWidth,
                      alignment: Alignment.bottomCenter,
                      image: AssetImage('assets/images/world_map_purp.png')))),
        ),

        // The background animation
        Positioned(
          top: animationTop,
          child: Visibility(
            visible: _showConnectedBackground(),
            child: AnimatedBuilder(
              builder: (context, child) => Opacity(
                  child: Container(
                    width: animationSize.width, height: animationSize.height,
                    child: FlareActor(
                      connectedAnimation,
                      fit: BoxFit.fitWidth,
                      animation: connectedAnimationName,
                      callback: (name) {
                        if (name == connectedAnimationIntroName) {
                          setState(() {
                            _showIntroAnimation = false;
                          });
                        }
                      },
                    ),
                  ),
                  opacity: _animOpacity.value),
              animation: _connectAnimController,
            ),
          ),
        ),

        // The page content including the button title, button, and route info when connected.
        buildPageContent(context),

        // Options bar with optional notification banner
        Align(
          alignment: Alignment.topCenter,
          child: Column(
            children: <Widget>[
              // The optional notification banner
              AnimatedSwitcher(
                child: NotificationBannerFactory.current() ?? Container(),
                transitionBuilder: (widget, anim) {
                  var tween =
                      Tween<Offset>(begin: Offset(0.0, -1.0), end: Offset.zero)
                          .animate(anim);
                  return SlideTransition(position: tween, child: widget);
                },
                duration: Duration(milliseconds: 200),
              ),
              // The options bar. (Animated builder allows the color transition).
              AnimatedBuilder(
                builder: (context, child) {
                  // https://stackoverflow.com/questions/45424621/inkwell-not-showing-ripple-effect
                  //Material
                  return OptionsBar(
                    color: _iconColor.value,
                    menuPressed: () {
                      Scaffold.of(context).openDrawer();
                    },
                    morePressed: () {},
                  );
                },
                animation: _connectAnimController,
              ),
            ],
          ),
        ),
      ],
    );
  }

  /// The page content including the button title, button, and route info when connected.
  Widget buildPageContent(BuildContext context) {
    var screenSize = MediaQuery.of(context).size;
    var buttonY = screenSize.height * 0.34;
    var buttonImageHeight = 142;

    return Stack(
      alignment: Alignment.center,
      fit: StackFit.expand,
      children: <Widget>[
        Positioned(
            top: buttonY - buttonImageHeight * 1.05,
            child: _buildStatusMessage(context)),
        Positioned(
          top: -screenSize.width / 2 + buttonY,
          width: screenSize.width,
          height: screenSize.width,
          child: ConnectButton(
            connectionStatus: OrchidAPI().connectionStatus,
            enabledStatus: BehaviorSubject.seeded(true),
            onConnectButtonPressed: _onConnectButtonPressed,
            onRerouteButtonPressed: _rerouteButtonPressed,
          ),
        ),
        Positioned(
          top: buttonY + buttonImageHeight * 1.11,
          child: Visibility(
              visible: _showConnectedBackground(),
              maintainSize: true,
              maintainAnimation: true,
              maintainState: true,
              child: RouteInfo()),
        ),
      ],
    );
  }

  Widget _buildStatusMessage(BuildContext context) {
    // Localize
    Map<OrchidConnectionState, String> connectionStateMessage = {
      OrchidConnectionState.NotConnected: 'Push to connect.',
      OrchidConnectionState.Connecting: 'Connecting...',
      //OrchidConnectionState.Connected: 'Orchid is running! 🙌',
      OrchidConnectionState.Connected: 'Orchid is running!',
    };

    String message = connectionStateMessage[_connectionState];
    Color color = (_connectionState == OrchidConnectionState.Connected
        ? AppColors.neutral_6 // light
        : AppColors.neutral_1); // dark

    return Container(
      // Note: the emoji changes the baseline so we give this a couple of pixels
      // Note: of extra hieght and bottom align it.
      height: 18.0,
      alignment: Alignment.bottomCenter,
      child: Text(message,
          style: AppText.connectButtonMessageStyle.copyWith(color: color)),
    );
  }

  void _onConnectButtonPressed() {
    // Toggle the current connection state
    switch (_connectionState) {
      case OrchidConnectionState.NotConnected:
        OrchidAPI().setConnected(true);
        break;
      case OrchidConnectionState.Connecting:
      case OrchidConnectionState.Connected:
        OrchidAPI().setConnected(false);
        break;
    }
  }

  void _rerouteButtonPressed() {
    OrchidAPI().reroute();
  }
}
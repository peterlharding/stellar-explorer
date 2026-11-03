// Starts the engine on visionOS, which the 5.8 launcher build cannot do alone.
//
// Its precompiled engine starts the game thread from IOSSceneDelegate, a UIKit
// scene delegate (bUseSceneBasedLifecycle). A visionOS app is a SwiftUI app,
// whose scenes SwiftUI delegates to itself, so IOSSceneDelegate is never called:
// the splash window waits for its configuration, opens an empty immersive space
// and closes. Here the first window scene to connect is handed to
// IOSSceneDelegate as UIKit would have.

#include "CoreMinimal.h"

#if PLATFORM_VISIONOS && UE_IOS_SCENE_LIFECYCLE

#include "IOS/IOSSceneDelegate.h"

namespace
{
	IOSSceneDelegate* GEngineSceneDelegate = nil;

	__attribute__((constructor)) void StartEngineWithFirstWindowScene()
	{
		[[NSNotificationCenter defaultCenter] addObserverForName:UISceneWillConnectNotification
			object:nil
			queue:[NSOperationQueue mainQueue]
			usingBlock:^(NSNotification* Notification)
			{
				UIScene* Scene = Notification.object;
				if (GEngineSceneDelegate != nil || ![Scene isKindOfClass:[UIWindowScene class]]
					|| [Scene.delegate isKindOfClass:[IOSSceneDelegate class]])
				{
					return;
				}
				// UIKit's connection options cannot be made outside UIKit. The
				// delegate only reads their URLs and user activities, which are
				// empty on a nil object.
				UISceneConnectionOptions* NoOptions = nil;
				GEngineSceneDelegate = [[IOSSceneDelegate alloc] init];
				[GEngineSceneDelegate scene:Scene willConnectToSession:Scene.session options:NoOptions];
			}];
	}
}

#endif

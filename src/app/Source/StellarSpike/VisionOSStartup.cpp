// Starts the engine on visionOS, which the 5.8 launcher build cannot do alone.
//
// Its precompiled engine starts the game thread from IOSSceneDelegate, a UIKit
// scene delegate (bUseSceneBasedLifecycle). A visionOS app is a SwiftUI app,
// whose scenes SwiftUI delegates to itself, so IOSSceneDelegate is never called:
// the splash window waits for its configuration, opens an empty immersive space
// and closes. Here the first window scene to connect is handed to
// IOSSceneDelegate as UIKit would have.
//
// The splash window's text is the engine's own too (SwiftMain.swift, fixed), so it is covered
// here with this app's, once the window is on screen (CoverSplash).

#include "CoreMinimal.h"

#if PLATFORM_VISIONOS && UE_IOS_SCENE_LIFECYCLE

#include "BuildStamp.h"
#include "IOS/IOSSceneDelegate.h"

namespace
{
	IOSSceneDelegate* GEngineSceneDelegate = nil;

	NSString* AsNSString(const TCHAR* Text)
	{
		return [[NSString alloc] initWithBytes:Text length:FCString::Strlen(Text) * sizeof(TCHAR) encoding:NSUTF16LittleEndianStringEncoding];
	}

	UILabel* MakeLabel(NSString* Text, UIFontTextStyle Style, UIColor* Colour, bool bBold)
	{
		UILabel* Label = [[UILabel alloc] init];
		Label.text = Text;
		Label.textAlignment = NSTextAlignmentCenter;
		UIFont* Font = [UIFont preferredFontForTextStyle:Style];
		Label.font = bBold ? [UIFont fontWithDescriptor:[Font.fontDescriptor fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold] size:0.0] : Font;
		Label.textColor = Colour;
		return Label;
	}

	/**
	 * Over the engine's splash window, whose text is its own and meant for its developers
	 * ("Unreal SwiftMain Splash Screen!", the immersion settings, a "Go Immersive!" countdown),
	 * this app's: its name, the version and build, and what is happening. The window still
	 * opens the immersive space and closes itself after three seconds. The version is the
	 * bundle's, which make version-set stamps (VersionInfo in DefaultEngine.ini), as the
	 * engine's settings are not read yet; the commit and branch are BuildStamp.inl's. Plain
	 * strings only: the engine's allocator may not be ready.
	 */
	void CoverSplash(UIWindowScene* Scene, int32 Tries)
	{
		UIView* Root = Scene.windows.firstObject.rootViewController.view;
		if (Root == nil)
		{
			// The window comes a moment after the scene: look again, for a second at most.
			if (Tries < 10)
			{
				dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 10), dispatch_get_main_queue(), ^{
					CoverSplash(Scene, Tries + 1);
				});
			}
			return;
		}
		NSString* Version = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
		NSString* Commit = AsNSString(STELLAR_SPIKE_COMMIT);
		const NSRange Plus = [Commit rangeOfString:@"+"];
		NSString* Short = Plus.location == NSNotFound ? [Commit substringToIndex:MIN((NSUInteger)7, Commit.length)]
			: [[Commit substringToIndex:MIN((NSUInteger)7, Plus.location)] stringByAppendingString:[Commit substringFromIndex:Plus.location]];

		// Opaque, to hide the engine's text, and night-sky dark; rounded as the window is.
		UIView* Cover = [[UIView alloc] init];
		Cover.backgroundColor = [UIColor colorWithRed:0.03 green:0.035 blue:0.05 alpha:1.0];
		Cover.layer.cornerRadius = 46.0;
		Cover.layer.cornerCurve = kCACornerCurveContinuous;
		Cover.translatesAutoresizingMaskIntoConstraints = NO;
		UIStackView* Lines = [[UIStackView alloc] initWithArrangedSubviews:@[
			MakeLabel(@"Stellar Explorer", UIFontTextStyleLargeTitle, [UIColor whiteColor], true),
			MakeLabel([NSString stringWithFormat:@"Version %@", Version != nil ? Version : @"unknown"], UIFontTextStyleTitle3, [UIColor lightGrayColor], false),
			MakeLabel([NSString stringWithFormat:@"build %@, %@", Short, AsNSString(STELLAR_SPIKE_BRANCH)], UIFontTextStyleFootnote, [UIColor grayColor], false),
		]];
		Lines.axis = UILayoutConstraintAxisVertical;
		Lines.alignment = UIStackViewAlignmentCenter;
		Lines.spacing = 8.0;
		Lines.translatesAutoresizingMaskIntoConstraints = NO;
		UILabel* Opening = MakeLabel(@"Opening the sky\u2026", UIFontTextStyleBody, [UIColor lightGrayColor], false);
		Opening.translatesAutoresizingMaskIntoConstraints = NO;
		[Cover addSubview:Lines];
		[Cover addSubview:Opening];
		[Root addSubview:Cover];
		[NSLayoutConstraint activateConstraints:@[
			[Cover.leadingAnchor constraintEqualToAnchor:Root.leadingAnchor],
			[Cover.trailingAnchor constraintEqualToAnchor:Root.trailingAnchor],
			[Cover.topAnchor constraintEqualToAnchor:Root.topAnchor],
			[Cover.bottomAnchor constraintEqualToAnchor:Root.bottomAnchor],
			[Lines.centerXAnchor constraintEqualToAnchor:Cover.centerXAnchor],
			[Lines.centerYAnchor constraintEqualToAnchor:Cover.centerYAnchor constant:-20.0],
			[Opening.centerXAnchor constraintEqualToAnchor:Cover.centerXAnchor],
			[Opening.bottomAnchor constraintEqualToAnchor:Cover.safeAreaLayoutGuide.bottomAnchor constant:-32.0],
		]];
	}

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
				// The splash window: the first window scene, an ordinary window, not the immersive space.
				if ([Scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication])
				{
					CoverSplash((UIWindowScene*)Scene, 0);
				}
			}];
	}
}

#endif

// Dessine l'icône de Grid Play (1024 x 1024) : le rectangle blanc qui passe au noir
// en diagonale du site, les joues en bois des plugins, et une grille de pads allumés.
// Compilation : clang -fobjc-arc -framework Cocoa Tools/make_icon.m -o make_icon && ./make_icon Resources/icon.png
#import <Cocoa/Cocoa.h>

static NSColor* rgb (int hex, CGFloat a) { return [NSColor colorWithSRGBRed: ((hex >> 16) & 255) / 255.0 green: ((hex >> 8) & 255) / 255.0 blue: (hex & 255) / 255.0 alpha: a]; }

int main (int argc, const char* argv[]) { @autoreleasepool {
    const int size = 1024;
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes: NULL pixelsWide: size pixelsHigh: size bitsPerSample: 8
                                                               samplesPerPixel: 4 hasAlpha: YES isPlanar: NO colorSpaceName: NSDeviceRGBColorSpace bytesPerRow: 0 bitsPerPixel: 0];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext: [NSGraphicsContext graphicsContextWithBitmapImageRep: rep]];
    [[NSColor clearColor] set];
    NSRectFillUsingOperation (NSMakeRect (0, 0, size, size), NSCompositingOperationClear);

    // Corps de l'icône (grille d'icônes macOS : 824 px, coins arrondis).
    NSRect body = NSMakeRect (100, 100, 824, 824);
    NSBezierPath* shape = [NSBezierPath bezierPathWithRoundedRect: body xRadius: 185 yRadius: 185];

    // Ombre portée
    NSShadow* shadow = [NSShadow new];
    shadow.shadowColor = [NSColor colorWithWhite: 0 alpha: 0.45];
    shadow.shadowBlurRadius = 28;
    shadow.shadowOffset = NSMakeSize (0, -12);
    [NSGraphicsContext saveGraphicsState];
    [shadow set];
    [rgb (0x0b0b0b, 1) set];
    [shape fill];
    [NSGraphicsContext restoreGraphicsState];

    [NSGraphicsContext saveGraphicsState];
    [shape addClip];
    // Dégradé en diagonale : blanc en haut à gauche, noir en bas à droite (comme les cartes du site).
    NSGradient* fade = [[NSGradient alloc] initWithColorsAndLocations:
        rgb (0xffffff, 1), 0.0, rgb (0xf4f4f2, 1), 0.28, rgb (0x6a6a6a, 1), 0.46, rgb (0x0b0b0b, 1), 0.6, rgb (0x0b0b0b, 1), 1.0, nil];
    [fade drawInRect: body angle: -45];

    // Joues en bois, comme les façades des plugins.
    for (int side = 0; side < 2; ++side)
    {
        NSRect cheek = NSMakeRect (side == 0 ? body.origin.x : NSMaxX (body) - 58, body.origin.y, 58, body.size.height);
        NSGradient* wood = [[NSGradient alloc] initWithStartingColor: rgb (0x6e4220, 1) endingColor: rgb (0x3e220e, 1)];
        [wood drawInRect: cheek angle: side == 0 ? 0 : 180];
        [[NSColor colorWithWhite: 0 alpha: 0.18] set];
        for (CGFloat y = cheek.origin.y + 20; y < NSMaxY (cheek); y += 46)
            NSRectFillUsingOperation (NSMakeRect (cheek.origin.x, y, cheek.size.width, 3), NSCompositingOperationSourceOver);
    }
    [NSGraphicsContext restoreGraphicsState];

    // Plaque sombre et grille de pads 3 x 3.
    NSRect plate = NSMakeRect (250, 250, 524, 524);
    [NSGraphicsContext saveGraphicsState];
    shadow.shadowBlurRadius = 34;
    shadow.shadowOffset = NSMakeSize (0, -16);
    shadow.shadowColor = [NSColor colorWithWhite: 0 alpha: 0.55];
    [shadow set];
    [rgb (0x161616, 1) set];
    [[NSBezierPath bezierPathWithRoundedRect: plate xRadius: 46 yRadius: 46] fill];
    [NSGraphicsContext restoreGraphicsState];

    // De haut en bas, de gauche à droite : orange, éteint, vert / éteint, orange, éteint / vert, jaune, éteint.
    const int colours[9] = { 0xe8862c, 0x2b2a28, 0x8fd35a, 0x2b2a28, 0xe8862c, 0x2b2a28, 0x8fd35a, 0xf2d34f, 0x2b2a28 };
    const CGFloat pad = 138, gap = 30, start = plate.origin.x + (plate.size.width - 3 * pad - 2 * gap) / 2;
    for (int i = 0; i < 9; ++i)
    {
        const int row = i / 3, col = i % 3;
        NSRect r = NSMakeRect (start + col * (pad + gap), NSMaxY (plate) - (plate.size.height - 3 * pad - 2 * gap) / 2 - (row + 1) * pad - row * gap, pad, pad);
        NSColor* c = rgb (colours[i], 1);
        NSBezierPath* p = [NSBezierPath bezierPathWithRoundedRect: r xRadius: 22 yRadius: 22];
        if (colours[i] != 0x2b2a28)
        {
            [NSGraphicsContext saveGraphicsState];
            NSShadow* glow = [NSShadow new];
            glow.shadowColor = [c colorWithAlphaComponent: 0.65];
            glow.shadowBlurRadius = 30;
            glow.shadowOffset = NSZeroSize;
            [glow set];
            [c set];
            [p fill];
            [NSGraphicsContext restoreGraphicsState];
            NSGradient* shine = [[NSGradient alloc] initWithStartingColor: [NSColor colorWithWhite: 1 alpha: 0.28] endingColor: [NSColor colorWithWhite: 1 alpha: 0.0]];
            [shine drawInBezierPath: p angle: -90];
        }
        else
        {
            [c set];
            [p fill];
        }
    }

    // Triangle « lecture » sur le pad du centre.
    NSRect centre = NSMakeRect (start + pad + gap, NSMaxY (plate) - (plate.size.height - 3 * pad - 2 * gap) / 2 - 2 * pad - gap, pad, pad);
    NSBezierPath* tri = [NSBezierPath bezierPath];
    [tri moveToPoint: NSMakePoint (NSMidX (centre) - 24, NSMidY (centre) + 34)];
    [tri lineToPoint: NSMakePoint (NSMidX (centre) - 24, NSMidY (centre) - 34)];
    [tri lineToPoint: NSMakePoint (NSMidX (centre) + 36, NSMidY (centre))];
    [tri closePath];
    [rgb (0x111111, 0.85) set];
    [tri fill];

    [NSGraphicsContext restoreGraphicsState];
    NSData* png = [rep representationUsingType: NSBitmapImageFileTypePNG properties: @{}];
    [png writeToFile: @(argv[1]) atomically: YES];
    printf ("Icône : %s\n", argv[1]);
}}

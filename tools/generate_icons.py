"""Generate the project's own handheld icon; Pillow is a build-time tool only."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "npe-sdl/platform"
image = Image.new("RGBA", (1024, 1024))
draw = ImageDraw.Draw(image)
svg = []

def rect(box, color, radius=0, stroke=None, width=1):
    draw.rounded_rectangle(box, radius, fill=color, outline=stroke, width=width)
    x,y,right,bottom = box
    svg.append(f'<rect x="{x}" y="{y}" width="{right-x}" height="{bottom-y}" rx="{radius}" fill="{color}"'
               + (f' stroke="{stroke}" stroke-width="{width}"' if stroke else '') + '/>')

def circle(x,y,r,color):
    draw.ellipse((x-r,y-r,x+r,y+r),fill=color)
    svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')

rect((0,0,1024,1024),"#18293d",190)
rect((148,70,876,950),"#e0e6dd",90,"#0d1925",18)
rect((202,156,822,592),"#344650",45)
rect((238,194,786,546),"#b7c89f",20)
glyphs = (
    ("10001","11001","11001","10101","10011","10011","10001"),
    ("11110","10001","10001","11110","10000","10000","10000"),
    ("11111","10000","10000","11110","10000","10000","11111"),
)
for letter,glyph in enumerate(glyphs):
    for y,row in enumerate(glyph):
        for x,pixel in enumerate(row):
            if pixel=="1":
                px=278+letter*164+x*26;py=278+y*26
                rect((px,py,px+23,py+23),"#293e32",2)
rect((250,704,442,768),"#293c50",12)
rect((314,640,378,832),"#293c50",12)
circle(662,718,57,"#3e7ea2")
circle(758,810,57,"#b34f58")
rect((456,858,564,877),"#819082",8)
rect((588,858,696,877),"#819082",8)
OUT.mkdir(parents=True, exist_ok=True)
(OUT/"app.svg").write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024">'+''.join(svg)+'</svg>\n',encoding="utf8")
image.save(OUT/"app.png")
image.save(OUT/"app.ico",sizes=[(n,n) for n in (16,24,32,48,64,128,256)])
image.save(OUT/"app.icns")
for density,size in (("mdpi",48),("hdpi",72),("xhdpi",96),("xxhdpi",144),("xxxhdpi",192)):
    directory=ROOT/"android/app/src/main/res"/f"mipmap-{density}"
    directory.mkdir(parents=True,exist_ok=True)
    image.resize((size,size),Image.Resampling.LANCZOS).save(directory/"ic_launcher.png")
print("Generated SVG, PNG, ICO, ICNS and Android launcher icons")

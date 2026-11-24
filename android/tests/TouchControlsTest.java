package com.helloclyde.npe;
import java.util.ArrayList;
import java.util.Arrays;
public final class TouchControlsTest {
    private static void require(boolean condition) { if(!condition)throw new AssertionError(); }
    public static void main(String[] args) {
        for(int[] size:new int[][]{{320,240},{320,480},{640,360},{800,360},{1280,720}}) {
            TouchControls.Layout layout=new TouchControls.Layout(size[0],size[1],100);
            for (int percent : new int[]{80,100,150}) {
                TouchControls.Layout scaled = new TouchControls.Layout(size[0],size[1],percent);
                require(scaled.left+scaled.size<=scaled.right);
                require(scaled.top+scaled.size<=size[1]);
                require(scaled.utilityTop+scaled.utilityHeight<=scaled.top);
                require(scaled.left+scaled.utilityUnit*2<=(size[0]-72)/2);
                require(scaled.unit>=40);
            }
            require(layout.unit>=48 && layout.left+layout.size<=layout.right);
            require(layout.top>=0&&layout.top+layout.size<=size[1]);
            require(layout.right+layout.size<=size[0]);
            require(layout.utilityTop+48<=layout.top);
            // Even short screens keep both utility groups clear of More.
            require(layout.left+layout.utilityUnit*2<=(size[0]-72)/2);
            require(size[0]-12-layout.utilityUnit*2>=(size[0]+72)/2);
            require(layout.utilityUnit-4>=48);
        }
        TouchControls.Layout high = new TouchControls.Layout(390,720,100,20);
        TouchControls.Layout low = new TouchControls.Layout(390,720,100,60);
        TouchControls.Layout edge = new TouchControls.Layout(390,720,150,100);
        require(high.utilityTop==144 && low.utilityTop==432 && high.size==low.size);
        require(edge.top+edge.size<=720 && edge.utilityTop>=8);
        TouchControls.Layout small = new TouchControls.Layout(800,360,80);
        TouchControls.Layout normal = new TouchControls.Layout(800,360,100);
        TouchControls.Layout large = new TouchControls.Layout(800,360,150);
        require(small.unit < normal.unit && normal.unit < large.unit);
        require(small.utilityHeight < normal.utilityHeight && normal.utilityHeight < large.utilityHeight);
        require(TouchControls.column(4)==2&&TouchControls.row(4)==1); // A right
        require(TouchControls.column(5)==1&&TouchControls.row(5)==2); // B bottom
        require(TouchControls.column(6)==1&&TouchControls.row(6)==0); // X top
        require(TouchControls.column(7)==0&&TouchControls.row(7)==1); // Y left
        ArrayList<String> events=new ArrayList<>();
        TouchControls.DirectionPad pad=new TouchControls.DirectionPad((action,down)->events.add(action+(down?"+":"-")));
        pad.down(7,78,10,156,156);pad.repeat();
        pad.move(7,145,78,156,156);pad.down(8,10,78,156,156);pad.up(8);
        require(pad.direction==1&&pad.pointer==7);
        pad.move(7,78,78,156,156);pad.move(7,10,78,156,156);pad.cancel();pad.repeat();
        require(events.equals(Arrays.asList("0+","0+","0-","1+","1-","3+","3-")));
        pad.down(9,78,145,156,156);pad.move(9,-1,145,156,156);require(pad.direction==-1);pad.up(9);
        require(pad.pointer==-1);
        System.out.println("Touch controls: Nintendo diamond, 5 screen sizes, 80–150 size presets, utility/More separation, hold/repeat, slide, deadzone, pointer ownership and cancel/release passed");
    }
}

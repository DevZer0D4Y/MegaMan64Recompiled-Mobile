#include "vr/tracking.h"
#include "vr/math.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include <cstdio>
static MMVrTracking frame(uint64_t sequence) {
    MMVrTracking value{}; value.abi=MM_VR_ABI; value.size=sizeof(value); value.sequence=sequence;
    value.focused=1; value.eyeCount=2; value.originGeneration=1;
    value.head.flags=3; value.head.orientation[3]=2; value.head.position[0]=float(sequence);
    for(int eye=0;eye<2;eye++) {
        value.eyes[eye].pose=value.head; value.eyes[eye].pose.position[0]=eye ? .032f : -.032f;
        value.eyes[eye].fovLeft=-.8f; value.eyes[eye].fovRight=.8f; value.eyes[eye].fovUp=.8f; value.eyes[eye].fovDown=-.8f;
        value.aim[eye]=value.head; value.grip[eye]=value.head;
    }
    return value;
}
int main() {
    static_assert(sizeof(MMVrPose)==32 && sizeof(MMVrEye)==48 && sizeof(MMVrTracking)==328);
    MMVrTracking output{}; mm_vr_clear_tracking(); assert(!mm_vr_read_tracking(&output,sizeof(output)));
    mm_vr_set_tracking_status(0,1); mm_vr_set_tracking_status(1,1);
    auto value=frame(1); value.trigger[0]=2; value.stick[1][0]=-2; mm_vr_submit_tracking(&value);
    assert(mm_vr_read_tracking(&output,sizeof(output))); assert(output.head.orientation[3]==1);
    assert(output.trigger[0]==1 && output.stick[1][0]==-1);
    assert(output.eyes[0].pose.position[0] < output.eyes[1].pose.position[0]);
    value=frame(2); value.aim[0].flags=0; value.trigger[0]=1; mm_vr_submit_tracking(&value);
    assert(mm_vr_read_tracking(&output,sizeof(output)) && output.trigger[0]==0);
    value=frame(3); value.focused=0; mm_vr_submit_tracking(&value); assert(!mm_vr_read_tracking(&output,sizeof(output)));
    value=frame(4); value.head.orientation[0]=std::numeric_limits<float>::quiet_NaN(); mm_vr_submit_tracking(&value);
    assert(!mm_vr_read_tracking(&output,sizeof(output)));
    value=frame(5); value.eyes[0].fovUp=value.eyes[0].fovDown; mm_vr_submit_tracking(&value);
    assert(!mm_vr_read_tracking(&output,sizeof(output)));
    value=frame(6); mm_vr_submit_tracking(&value); value=frame(5); mm_vr_submit_tracking(&value);
    assert(mm_vr_read_tracking(&output,sizeof(output)) && output.sequence==6);
    value=frame(7); value.abi=99; mm_vr_submit_tracking(&value);
    assert(mm_vr_read_tracking(&output,sizeof(output)) && output.sequence==6);
    assert(!mm_vr_read_tracking(&output,sizeof(output)-1));
    std::this_thread::sleep_for(std::chrono::milliseconds(270)); assert(!mm_vr_read_tracking(&output,sizeof(output)));
    mm_vr_clear_tracking(); std::atomic<bool> done{false};
    std::thread writer([&] { for(uint64_t i=1;i<=10000;i++){auto sample=frame(i);mm_vr_submit_tracking(&sample);}done=true; });
    while(!done.load()) if(mm_vr_read_tracking(&output,sizeof(output))) assert(output.head.position[0]==float(output.sequence));
    writer.join(); assert(mm_vr_read_tracking(&output,sizeof(output)) && output.sequence==10000);
    mm_vr_clear_tracking(); assert(!mm_vr_read_tracking(&output,sizeof(output)));
    mm_vr_set_tracking_status(0,0); value=frame(10001); mm_vr_submit_tracking(&value);
    assert(!mm_vr_read_tracking(&output,sizeof(output)));
    mm_vr_set_tracking_status(0,1); mm_vr_set_tracking_status(1,0); mm_vr_submit_tracking(&value);
    assert(!mm_vr_read_tracking(&output,sizeof(output)));
    mm_vr_set_tracking_status(1,1); mm_vr_submit_tracking(&value);
    assert(mm_vr_read_tracking(&output,sizeof(output)));
    std::atomic<bool> stop{false};
    std::thread producer([&] { uint64_t seq=10002; while(!stop.load()){auto sample=frame(seq++);mm_vr_submit_tracking(&sample);} });
    mm_vr_set_tracking_status(0,0);
    for (int i=0;i<1000;++i) assert(!mm_vr_read_tracking(&output,sizeof(output)));
    stop=true; producer.join();
    auto close=[](float a,float b){assert(std::abs(a-b)<1e-4f);};
    auto point=[](const immersive::Matrix& m, float x,float y,float z) {
        const auto* a=m.value; float w=a[12]*x+a[13]*y+a[14]*z+a[15];
        return immersive::Vec3{(a[0]*x+a[1]*y+a[2]*z+a[3])/w,(a[4]*x+a[5]*y+a[6]*z+a[7])/w,(a[8]*x+a[9]*y+a[10]*z+a[11])/w};
    };
    auto eye=frame(1).eyes[0]; eye.pose.orientation[3]=1;
    immersive::Matrix projection{}; assert(immersive::projection(eye,.05f,1000,projection));
    close(point(projection,std::tan(eye.fovLeft),0,-1).x,-1);
    close(point(projection,std::tan(eye.fovRight),0,-1).x,1);
    close(point(projection,0,std::tan(eye.fovUp),-1).y,-1);
    close(point(projection,0,std::tan(eye.fovDown),-1).y,1);
    close(point(projection,0,0,-.05f).z,0); close(point(projection,0,0,-1000).z,1);
    assert(!immersive::projection(eye,0,1000,projection));
    auto left=immersive::view(eye.pose); eye.pose.position[0]=.032f; auto right=immersive::view(eye.pose);
    assert(point(left,0,0,-1).x>point(right,0,0,-1).x);
    MMVrPose aim{}; aim.flags=3; aim.orientation[1]=std::sqrt(.5f); aim.orientation[3]=std::sqrt(.5f);
    immersive::Vec3 origin{},direction{}; assert(immersive::aimRay(aim,origin,direction));
    close(direction.x,-1); close(direction.y,0); close(direction.z,0);
    aim.flags=0; assert(!immersive::aimRay(aim,origin,direction));
    std::puts("Tracking ABI, stereo poses, lost tracking, stale data, sanitization concurrent snapshots, suspension races, eye projection, stereo separation and aim-ray tests passed");
}

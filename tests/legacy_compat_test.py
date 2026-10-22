"""Exercise Papyrus callbacks and the background dispatcher."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/Plugin.cpp').read_text()
start = source.index('void backgroundUpdatePapyrus(')
bridge = source[start:source.index('bool resumeMatch()', start)]

api_source = (root / 'include/NativeCollectionAPI.h').read_text()
queue = api_source[api_source.index('template<class Task> void queue('):api_source.index('inline int apiVersion(')]

# Compile the actual callbacks, substituting only the SKSE task queue and the
# engine refresh. Callback calls leave the dispatcher idle.
harness = r'''
#include <atomic>
#include <cassert>
#include <deque>
#include <functional>
#include <stdexcept>
#include <thread>
#include <vector>
#include "NpcFactionTracking.h"
namespace RE { struct StaticFunctionTag{}; struct Actor { int state=42; }; }
namespace SKSE {
struct Tasks {
 std::deque<std::function<void()>> pending;
 void AddTask(std::function<void()> f){pending.push_back(std::move(f));}
 void run(){auto f=std::move(pending.front());pending.pop_front();f();}
};
Tasks tasks;
Tasks* GetTaskInterface(){return &tasks;}
}
namespace collectionIntegration {
inline void publish() {}
''' + queue + r'''
}
namespace campaign { ttcg::NpcFactionTracking npcFactionTracking; }
int updates=0;bool fail=false;
void backgroundUpdate(){if(fail)throw std::runtime_error("test");++updates;}
''' + bridge + r'''
int main(){
 RE::Actor actor;
 for(int n=0;n<100;++n){
  assert(legacyPapyrus::nextMaster(nullptr)==nullptr);
  legacyPapyrus::masterArrived(nullptr,&actor);
  legacyPapyrus::masterResponse(nullptr,&actor,n%2);
  legacyPapyrus::masterArrived(nullptr,nullptr);
  legacyPapyrus::masterResponse(nullptr,nullptr,false);
 }
 assert(updates==0 && actor.state==42 && SKSE::tasks.pending.empty());
 // Concurrent Papyrus callers still queue only one refresh.
 std::vector<std::thread> callers;
 for(int n=0;n<32;++n)callers.emplace_back([]{backgroundUpdatePapyrus(nullptr);});
 for(auto& t:callers)t.join();
 assert(SKSE::tasks.pending.size()==1);
 SKSE::tasks.run();assert(updates==1);
 backgroundUpdatePapyrus(nullptr);
 campaign::npcFactionTracking.reset(); // Load another save before execution.
 SKSE::tasks.run();assert(updates==1);
 backgroundUpdatePapyrus(nullptr);SKSE::tasks.run();assert(updates==2);
 // A failed refresh must not permanently block later requests.
 fail=true;backgroundUpdatePapyrus(nullptr);
 try { SKSE::tasks.run();assert(false); } catch(const std::runtime_error&) {}
 fail=false;backgroundUpdatePapyrus(nullptr);SKSE::tasks.run();assert(updates==3);
}
'''
with tempfile.TemporaryDirectory(prefix='ttcg-legacy-') as directory:
    cpp = Path(directory) / 'callbacks.cpp'
    exe = Path(directory) / 'callbacks'
    cpp.write_text(harness)
    subprocess.run(['c++', '-std=c++20', '-pthread', '-I'+str(root/'include'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

def papyrus(name):
    return re.sub(r';[^\n]*', '', (root/'Scripts/Source'/f'{name}.psc').read_text()).strip()

for name in ('TTCG_MasterAcceptFragment', 'TTCG_MasterDeclineFragment'):
    assert re.fullmatch(rf'Scriptname {name} extends TopicInfo Hidden\s+'
                       r'Function Fragment_0\(ObjectReference akSpeakerRef\)\s+EndFunction', papyrus(name))
encounters = papyrus('TTCG_EncountersQuest')
assert 'Bool pathing = false' in encounters
assert not any(old in encounters for old in ('NextMaster', 'MasterArrived', 'PathToReference', 'EvaluatePackage'))
update = encounters.split('Event OnUpdate()', 1)[1]
assert update.count('RegisterForSingleUpdate') == 1
assert update.index('BackgroundUpdate()') < update.index('RegisterForSingleUpdate(30.0)')
assert 'scanMasterApproach' not in source
print('Papyrus callbacks and background dispatch tests passed')

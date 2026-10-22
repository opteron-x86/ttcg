"""Exercise the production recovery guard with SKSE's encoded bool payload."""
from pathlib import Path
import subprocess
import tempfile
import re

root = Path(__file__).resolve().parents[1]
source = (root / 'src/Plugin.cpp').read_text()
guard = re.search(r'if\((msg->type!=SKSE::MessagingInterface::kPreLoadGame&&.*?)\) \{\s*// SKSE passes', source, re.S)
assert guard, 'Collection load recovery guard not found'
code = r'''
#include <cassert>
namespace SKSE {struct MessagingInterface {enum {kPreLoadGame,kNewGame,kPostLoadGame};};}
struct Message {int type;void* data;};
bool recover(Message* msg){return GUARD;}
int main(){
    Message loaded{SKSE::MessagingInterface::kPostLoadGame,reinterpret_cast<void*>(1)};
    Message failed{SKSE::MessagingInterface::kPostLoadGame,nullptr};
    Message before{SKSE::MessagingInterface::kPreLoadGame,reinterpret_cast<void*>(1)};
    Message created{SKSE::MessagingInterface::kNewGame,nullptr};
    assert(recover(&loaded)&&!recover(&failed)&&!recover(&before)&&recover(&created));
}
'''.replace('GUARD', guard[1])
with tempfile.TemporaryDirectory(prefix='tessera-load-') as d:
    cpp = Path(d) / 'guard.cpp'
    exe = Path(d) / 'guard'
    cpp.write_text(code)
    subprocess.run(['c++', '-std=c++20', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Collection load recovery: encoded SKSE success, failed load, preload and new game passed')

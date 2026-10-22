import {chooseMove} from './engine.mjs';
self.onmessage = ({data}) => {
  try {self.postMessage({id: data.id, move: chooseMove(data.game)});}
  catch (error) {self.postMessage({id: data.id, error: error.message});}
};

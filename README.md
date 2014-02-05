Allows user to get/set string keys/values from skse co-save (tied to savegame)
from scaleform

Sort of similar to localStorage in html5

strings *cannot* contain null values

    intrinsic class skse {
      static var plugins:Object;
    }

    // store a string
    function skse.plugins.junk_serialization.SetData(key:String, value:String):Void;
    // retrieve string
    function skse.plugins.junk_serialization.GetData(key:String):String;
    // remove data associated with key
    function skse.plugins.junk_serialization.Remove(key:String):Void;
    
    // parses supplied JSON string, undefined on error
    function skse.plugins.junk_serialization.parse(json:String):Object;
    
    // returns JSON representation of value passed
    function skse.plugins.junk_serialization.stringify(object:Object[, prettyPrint:Boolean=false]):String;

Provides de/serialization of objects to and from JSON. There is no equivalent
to reviver for parse, and no equivalent to replacer for stringify. If you have
an object with cyclic references, or another error occurs, or `object` is 
undefined stringify will return undefined.


I added two more functions SetObjects and GetObjects, they store and load JSON
strings to the storage without intermediary string copies to actionscript.

    function skse.plugins.junk_serialization.SetObjects(key:String, object:Object):Boolean

The key is the same as `SetData`

    function skse.plugins.junk_serialization.GetObjects(key:String):Object

This will parse JSON at `key` and return it as an object.

#include <dlfcn.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

#define LOG_TAG "newmodpe"

#include "log.h"
#include "access.h"
#include "hook.h"

#define MCPE_DATA_PATH "/sdcard/games/com.mojang/NewModPEDex.dex"
#define MCPE_DATA_PATH_DUMMY "/data/data/net.zhuoweizhang.mcpelauncher.pro/files/"

static bool dexLoaded = false;

extern JavaVM* bl_JavaVM;
extern jclass bl_scriptmanager_class;

// NO CLASS FINDING IN THREAD
static jclass List;
static jclass MainActivity;
static jclass N_Player;
static jclass N_Proxy;
static jclass Object;
static jclass Reference;
static jclass ScriptState;
static jclass ScriptableObject;
static jclass Set;
static jclass Utils;

void initRhinoClassesAfterScriptLoad() {
    if (dexLoaded) {
        JNIEnv* env;
        int attachStatus = bl_JavaVM->GetEnv((void**) &env, JNI_VERSION_1_2);
        if (attachStatus == JNI_EDETACHED)
            bl_JavaVM->AttachCurrentThread(&env, NULL);
        jobject mainActivity = env->CallObjectMethod(
            env->GetStaticObjectField(MainActivity, env->GetStaticFieldID(MainActivity, "currentMainActivity", "Ljava/lang/ref/WeakReference;"))
            , env->GetMethodID(Reference, "get", "()Ljava/lang/Object;")
        );
        jfieldID displayMetricsGet = env->GetFieldID(MainActivity, "displayMetrics", "Landroid/util/DisplayMetrics;");
        jobject scripts = env->GetStaticObjectField(bl_scriptmanager_class, env->GetStaticFieldID(bl_scriptmanager_class, "scripts", "Ljava/util/List;"));
        jmethodID sizeGet = env->GetMethodID(List, "size", "()I");
        jint scriptCount = env->CallIntMethod(
            env->CallStaticObjectMethod(Utils, env->GetStaticMethodID(Utils, "getEnabledScripts", "()Ljava/util/Set;"))
            , env->GetMethodID(Set, "size", "()I")
        );
        while (env->CallIntMethod(scripts, sizeGet) < scriptCount);
        jint size = env->CallIntMethod(scripts, sizeGet);
        jmethodID listGet = env->GetMethodID(List, "get", "(I)Ljava/lang/Object;");
        jfieldID getScope = env->GetFieldID(ScriptState, "scope", "Lorg/mozilla/javascript/Scriptable;");
        jmethodID hasProperty = env->GetStaticMethodID(ScriptableObject, "hasProperty", "(Lorg/mozilla/javascript/Scriptable;Ljava/lang/String;)Z");
        jmethodID defineClass = env->GetStaticMethodID(ScriptableObject, "defineClass", "(Lorg/mozilla/javascript/Scriptable;Ljava/lang/Class;Z)V");
        LOGE("FUCK");

        for (int i = 0; i < size; i++) {
            LOGE("FUCK");
            jobject scope = env->GetObjectField(env->CallObjectMethod(scripts, listGet, i), getScope);
            if (!env->CallStaticBooleanMethod(ScriptableObject, hasProperty, scope, env->NewStringUTF("N_Player")))
                env->CallStaticVoidMethod(ScriptableObject, defineClass, scope, N_Player, true);
            if (!env->CallStaticBooleanMethod(ScriptableObject, hasProperty, scope, env->NewStringUTF("N_Proxy")))
                env->CallStaticVoidMethod(ScriptableObject, defineClass, scope, N_Proxy, true);
        }

        env->CallStaticVoidMethod(
            bl_scriptmanager_class
            , env->GetStaticMethodID(bl_scriptmanager_class, "callScriptMethod", "(Ljava/lang/String;[Ljava/lang/Object;)V")
            , env->NewStringUTF("onNewModPELoaded")
            , env->NewObjectArray(0, Object, NULL)
        );
        if (attachStatus == JNI_EDETACHED)
            bl_JavaVM->DetachCurrentThread();
    }
}

class BinaryStream;
enum class CommandPermissionLevel;
class CompoundTag;
class EntityDefinitionGroup;
class EntityDefinitionIdentifier;
class NetEventCallback;
class NetworkHandler;
class NetworkIdentifier;
class PermissionsHandler;

// Size : 12
class Packet {
    public:
    // void** vtable; // 0
    char filler1[8]; // 4

    public:
    virtual ~Packet();
    virtual int getId() const = 0;
    virtual void write(BinaryStream&) const = 0;
    virtual void read(BinaryStream&) = 0;
    virtual void handle(NetworkIdentifier const&, NetEventCallback const&) const = 0;
    virtual bool disallowBatching();
};

// Size : 4
class MinecraftPackets {
    public:
    Packet* retval; // 0

    public:
    Packet* createPacket(int);
};

typedef long long EntityRuntimeID;
typedef long long EntityUniqueID;

// Size : 24
class InteractPacket : public Packet {
    public:
    unsigned char type; // 12
    EntityRuntimeID runtimeID; // 16

    public:
    virtual ~InteractPacket();
    virtual int getId() const;
    virtual void write(BinaryStream&) const;
    virtual void read(BinaryStream&);
    virtual void handle(NetworkIdentifier const&, NetEventCallback const&) const;
};

enum class TextPacketType {
    RAW,
    CHAT,
    TRANSLATION,
    POPUP,
    TIP,
    SYSTEM,
    WHISPER
};

// Size : 36
class TextPacket : public Packet {
    public:
    TextPacketType type; // 12
    std::string sender; // 16
    std::string msg; // 20
    std::vector<std::string> params; // 24

    public:
    TextPacket(TextPacketType type, std::string const& sender, std::string const& msg, std::vector<std::string> const& params) : type(type), sender(sender), msg(msg), params(params) {}

    virtual ~TextPacket() {}
    virtual int getId() const;
    virtual void write(BinaryStream&) const;
    virtual void read(BinaryStream&);
    virtual void handle(NetworkIdentifier const&, NetEventCallback const&) const;

    static TextPacket* createRaw(std::string const&);
    static TextPacket* createSystemMessage(std::string const&);
    static TextPacket* createTranslated(std::string const&, std::vector<std::string> const&);
};

class Player;

class BatchedPacketSender {
    public:
    virtual ~BatchedPacketSender();
    virtual void send(Packet const&);
    virtual void send(NetworkIdentifier const&, Packet const&);
    virtual void sendBroadcast(NetworkIdentifier const&, Packet const&);
    virtual void sendBroadcast(Packet const&);
    virtual void flush(NetworkIdentifier const&);

    BatchedPacketSender(NetworkHandler&);
    /* BatchPacket* */ void _getBatch(NetworkIdentifier const&);
    bool _playerExists(NetworkIdentifier const&) const;
    void _queuePacket(NetworkIdentifier const&, Packet const&);
    void addLoopbackCallback(NetEventCallback &);
    void removeLoopbackCallback(NetEventCallback &);
    void setPlayerList(std::vector<std::unique_ptr<Player>> const*);
    void update();
};

typedef BatchedPacketSender PacketSender;

class Level {
    public:
    PacketSender* getPacketSender() const;
};

class Entity {
    public:
    float distanceTo(Entity const&) const;
    Level* getLevel();
    Level* getLevel() const;
    EntityRuntimeID getRuntimeID() const;
    EntityUniqueID getUniqueID() const;
    bool hasRuntimeID() const;
};

class Mob : public Entity {
    public:
    virtual ~Mob();
    Mob(EntityDefinitionGroup&, EntityDefinitionIdentifier const&);
    Mob(Level&);
};

class Player : public Mob {
    public:
    void attack(Entity&);
};

enum class EntityType {
    PLAYER = 0x100 | 63
};

class EntityClassTree {
    public:
    static bool isInstanceOf(Entity const&, EntityType);
};

class Minecraft {
    public:
    PacketSender* getPacketSender();
};

class MinecraftClient {
    public:
    Minecraft* getServer();
    void init();
    void onTick(int, int);
};

// Size : 24
class Abilities {
    public:
    bool invulnerable; // 0
    bool flying; // 1
    bool mayfly; // 2
    bool instabuild; // 3
    bool wtf; // 4
    bool lightning; // 5
    float walkingSpeed; // 8
    float flyingSpeed; // 12
    bool worldBuilder; // 16
    PermissionsHandler* permissions; // 20

    public:
    Abilities();
    void addSaveData(CompoundTag&) const;
    CommandPermissionLevel getCommandPermissions() const;
    float getFlyingSpeed() const;
    float getWalkingSpeed() const;
    bool isFlying() const;
    bool isWorldBuilder() const;
    void loadSaveData(CompoundTag&);
    void setCommandPermissions(CommandPermissionLevel);
    void setFlyingSpeed(float);
    void setWalkingSpeed(float);
    void setWorldBuilder(bool);
};

static struct {
    bool enable;
    bool hitMob;
    int distance;
} roundAttackConfig = { false, false, 12 };

static std::vector<Mob*> mobs;

static Mob* (*Mob$Mob_1_real)(Mob*, EntityDefinitionGroup*, EntityDefinitionIdentifier const*);
static Mob* Mob$Mob_1(Mob* mob, EntityDefinitionGroup* group, EntityDefinitionIdentifier const* identifier) {
    Mob$Mob_1_real(mob, group, identifier);
    if (std::find(mobs.begin(), mobs.end(), mob) == mobs.end()) mobs.push_back(mob);
    return mob;
}

static Mob* (*Mob$Mob_2_real)(Mob*, Level*);
static Mob* Mob$Mob_2(Mob* mob, Level* level) {
    Mob$Mob_2_real(mob, level);
    if (std::find(mobs.begin(), mobs.end(), mob) == mobs.end()) mobs.push_back(mob);
    return mob;
}

static Mob* (*Mob$dMob_real)(Mob*);
static Mob* Mob$dMob(Mob* mob) {
    mobs.erase(std::remove(mobs.begin(), mobs.end(), mob), mobs.end());
	Mob$dMob_real(mob);
    return mob;
}

static void (*MinecraftClient$onTick_real)(MinecraftClient*, int, int);
static void MinecraftClient$onTick(MinecraftClient* client, int idk1, int idk2) {
    MinecraftClient$onTick_real(client, idk1, idk2);
    if (!roundAttackConfig.enable)
        return;
    Player* thisPlayer = access(client, Player*, 0x120);
    if (thisPlayer == NULL)
        return;
    MinecraftPackets packetProvider;
    for (Mob* const& target : mobs)
        if (target != thisPlayer
            && target != NULL
            && (roundAttackConfig.hitMob || EntityClassTree::isInstanceOf(*target, EntityType::PLAYER))
            && target->distanceTo(*thisPlayer) <= roundAttackConfig.distance) {
            thisPlayer->attack(*target);
            InteractPacket* pk = static_cast<InteractPacket*>(packetProvider.createPacket(0x22));
            pk->type = 2;
            pk->runtimeID = access(target, EntityRuntimeID, 0xC60);
            // client->getServer()->getPacketSender()->send(*pk);
            thisPlayer->getLevel()->getPacketSender()->send(*pk);
        }
}

typedef bool (*isLocalPlayer_t)(Player*);
inline bool isLocalPlayer(Player* player) {
    return access(access(player, void**, 0), isLocalPlayer_t, 0x4AC)(player);
}

inline std::string getName(Player* player) {
    return access(player, std::string, 0xEBC);
}

JNIEXPORT void JNICALL nativeSetRoundAttack(JNIEnv* env, jclass clazz, jboolean enable, jboolean hitMob, jint distance) {
    roundAttackConfig.enable = enable;
    roundAttackConfig.hitMob = hitMob;
    roundAttackConfig.distance = distance;
}

JNIEXPORT void JNICALL nativeSetCanFly(JNIEnv* env, jclass clazz, jboolean enable) {
    for (Mob* const& mob : mobs)
        if (EntityClassTree::isInstanceOf(*mob, EntityType::PLAYER)
            && isLocalPlayer(static_cast<Player*>(mob))) {
            access(mob, Abilities, 0xEC0).mayfly = enable;
            break; // ONE LOCAL PLAYER
        }
}

JNIEXPORT void JNICALL nativeSendChat(JNIEnv* env, jclass clazz, jstring msg, jboolean hasSender) {
    const char* chars = env->GetStringUTFChars(msg, NULL);
    std::string _msg(chars);
    env->ReleaseStringUTFChars(msg, chars);
    for (Mob* const& mob : mobs)
        if (EntityClassTree::isInstanceOf(*mob, EntityType::PLAYER)
            && isLocalPlayer(static_cast<Player*>(mob))) {
            TextPacket pk(TextPacketType::CHAT, hasSender ? access(static_cast<Player*>(mob), std::string, 0xEBC) : "", _msg, {});
            mob->getLevel()->getPacketSender()->send(pk);
            break; // ONE LOCAL PLAYER
        }
    LOGE("C");
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    FILE* dexFile;

    if ((dexFile = fopen(MCPE_DATA_PATH, "r")) != NULL) {
        fclose(dexFile);
        dexLoaded = true;
        JNIEnv* env;
        int attachStatus = bl_JavaVM->GetEnv((void**) &env, JNI_VERSION_1_2);
        if (attachStatus == JNI_EDETACHED)
            bl_JavaVM->AttachCurrentThread(&env, NULL);
        jclass classCls = env->FindClass("java/lang/Class");
        jobject classLoader = env->CallObjectMethod(
            env->FindClass("com/mojang/minecraftpe/MainActivity")
            , env->GetMethodID(classCls, "getClassLoader", "()Ljava/lang/ClassLoader;")
        );
        jclass dexClassLoaderCls = env->FindClass("dalvik/system/DexClassLoader");
        jobject dexClassLoader = env->NewObject(
            dexClassLoaderCls
            , env->GetMethodID(dexClassLoaderCls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V")
            , env->NewStringUTF(MCPE_DATA_PATH)
            , env->NewStringUTF(MCPE_DATA_PATH_DUMMY)
            , NULL
            , classLoader
        );
        jclass n_playerCls = (jclass) env->CallStaticObjectMethod(
            classCls
            , env->GetStaticMethodID(classCls, "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;")
            , env->NewStringUTF("com.rmpi.newmodpe.N_Player")
            , true
            , dexClassLoader
        );
        JNINativeMethod implementation[] = {
            { "nativeSetRoundAttack", "(ZZI)V", (void*) *nativeSetRoundAttack }
            , { "nativeSetCanFly", "(Z)V", (void*) *nativeSetCanFly }
            , { "nativeSendChat", "(Ljava/lang/String;Z)V", (void*) *nativeSendChat }
        };
        env->RegisterNatives(n_playerCls, implementation, sizeof(implementation) / sizeof(JNINativeMethod));
        N_Player = (jclass) env->NewGlobalRef(n_playerCls);
        env->DeleteLocalRef(n_playerCls);
        jclass n_proxyCls = (jclass) env->CallStaticObjectMethod(
            classCls
            , env->GetStaticMethodID(classCls, "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;")
            , env->NewStringUTF("com.rmpi.newmodpe.N_Proxy")
            , true
            , dexClassLoader
        );
        N_Proxy = (jclass) env->NewGlobalRef(n_proxyCls);
        env->DeleteLocalRef(n_proxyCls);
        jclass scriptStateCls = env->FindClass("net/zhuoweizhang/mcpelauncher/ScriptManager$ScriptState");
        ScriptState = (jclass) env->NewGlobalRef(scriptStateCls);
        env->DeleteLocalRef(scriptStateCls);
        jclass scriptableObjectCls = env->FindClass("org/mozilla/javascript/ScriptableObject");
        ScriptableObject = (jclass) env->NewGlobalRef(scriptableObjectCls);
        env->DeleteLocalRef(scriptableObjectCls);
        jclass mainActivityCls = env->FindClass("com/mojang/minecraftpe/MainActivity");
        MainActivity = (jclass) env->NewGlobalRef(mainActivityCls);
        env->DeleteLocalRef(mainActivityCls);
        jclass referenceCls = env->FindClass("java/lang/ref/Reference");
        Reference = (jclass) env->NewGlobalRef(referenceCls);
        env->DeleteLocalRef(referenceCls);
        jclass listCls = env->FindClass("java/util/List");
        List = (jclass) env->NewGlobalRef(listCls);
        env->DeleteLocalRef(listCls);
        jclass utilsCls = env->FindClass("net/zhuoweizhang/mcpelauncher/Utils");
        Utils = (jclass) env->NewGlobalRef(utilsCls);
        env->DeleteLocalRef(utilsCls);
        jclass setCls = env->FindClass("java/util/Set");
        Set = (jclass) env->NewGlobalRef(setCls);
        env->DeleteLocalRef(setCls);
        jclass objectCls = env->FindClass("java/lang/Object");
        Object = (jclass) env->NewGlobalRef(objectCls);
        env->DeleteLocalRef(objectCls);
        if (attachStatus == JNI_EDETACHED)
            bl_JavaVM->DetachCurrentThread();
    }

    hookSymbol("_ZN3MobC2ER21EntityDefinitionGroupRK26EntityDefinitionIdentifier", Mob$Mob_1);
    hookSymbol("_ZN3MobC2ER5Level", Mob$Mob_2);
    hookSymbol("_ZN3MobD2Ev", Mob$dMob);
    hookSymbol("_ZN15MinecraftClient6onTickEii", MinecraftClient$onTick);
    std::thread doAfterScriptLoad(initRhinoClassesAfterScriptLoad);
    doAfterScriptLoad.detach();
	return JNI_VERSION_1_2;
}
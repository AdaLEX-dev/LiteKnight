# NPC Dialogue

## Recovered data

`TalkCharNpc` (SerializedMonoBehaviour, ItemInterface) serializes:

```text
char_npc_type, audio_clip, char_index, talk_name,
talk_string1, talk_string2, state, can_use, has_trigger,
the_object, drinkDistribute
```

`SequenceTalkTrigger` serializes `npc_name`, `height`, `TalkInfo[] talk_list`,
where `TalkInfo` is `{ talk_content, talk_time }`.

`TrainerTalkInfo/TrainerTalkGroup` provide `talk`, `dur_time`, loop groups for
the trainer/pet trainer.

## Native state

Not yet implemented. Character `talk` objects and their colliders exist in the
HeroRoom hierarchy; the native runtime only exposes the character-select marker
and does not display the recovered strings. The `biaoqing_16` speech/tab icons
were removed from the default room per the 0.5.4 cleanup request.

## Next implementation

1. Extract per-character `TalkCharNpc`/`SequenceTalkTrigger` strings into scene
   metadata.
2. Add an E interaction marker per unselected character NPC.
3. Reuse the recovered pixel font and a speech-bubble frame; advance through
   `talk_list` with `talk_time` and close on Escape/E.
4. Apply `TalkCharNpc.drinkDistribute` only where the item distribution data is
   fully recovered.

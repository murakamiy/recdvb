BEGIN {
    FS = ":"
}

{
    channel_id = $4
    channel_name = $2

    split($3, array, "|")
    split(array[2], frequency_array, "=")
    split(array[3], stream_id_array, "=")
    frequency = frequency_array[2]
    stream_id = stream_id_array[2]

    printf("{ %s, \"%s\", %s, %s },\n", channel_id, channel_name, frequency, stream_id)
}

if [ -z ${CYBER_PATH} ];then
    export CYBER_PATH=/usr/local/apollo/cyber/
    echo "CYBER_PATH not given. Use default path: ${CYBER_PATH}"
else
    echo "Use given CYBER_PATH path: ${CYBER_PATH}"
fi

if [ -z ${CRDC_WS} ];then
    export CRDC_WS=../
    echo "CRDC_WS not given. Use default path ${CRDC_WS}"
else
    echo "Use given CRDC_WS path: ${CRDC_WS}"
fi

if [ -z $1 ];then
    echo "No config given. Use the default."
    ./framework_main --alsologtostderr true --stderrthreshold 3 --v 0 --minloglevel 0 --colorlogtostderr true
else
    echo "Use the given config $1"
    ./framework_main --config_file $1 --alsologtostderr true --stderrthreshold 3 --v 0 --minloglevel 0 --colorlogtostderr true
fi
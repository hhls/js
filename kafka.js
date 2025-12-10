import {SCHEMA_TYPE_STRING, SchemaRegistry, Writer} from 'k6/x/kafka';

const KAFKA_BROKERS = (__ENV.KAFKA_BROKERS || 'alikafka-post-public-intl-sg-d6a4erz0a02-1-vpc.alikafka.aliyuncs.com:9092,alikafka-post-public-intl-sg-d6a4erz0a02-2-vpc.alikafka.aliyuncs.com:9092,alikafka-post-public-intl-sg-d6a4erz0a02-3-vpc.alikafka.aliyuncs.com:9092').split(',');
const KAFKA_TOPIC = __ENV.KAFKA_TOPIC || 'FUTURES_DEPTH_DEAL_ORDERS';
const TARGET = parseInt(__ENV.TARGET) || 50;
const ENABLE_LOG = __ENV.ENABLE_LOG === 'true';

function log(...args) {
    if (ENABLE_LOG) console.log(...args);
}

export const options = {
    scenarios: {
        my_ramping_scenario: {
            executor: 'ramping-arrival-rate',
            startRate: 10,
            timeUnit: '1s',
            preAllocatedVUs: 50,
            maxVUs: 100,
            gracefulStop: '10s',
            stages: [{duration: '120s', target: TARGET / 2}, {duration: '120s', target: TARGET}, {
                duration: '300s', target: TARGET
            }, {duration: '5s', target: 0},],
        },
    },
};

const writer = new Writer({
    brokers: KAFKA_BROKERS, topic: KAFKA_TOPIC,
});

const schemaRegistry = new SchemaRegistry();

export default function () {
    const timestamp = Date.now();
    const key = `key-${timestamp}`;

    const message = {
        "shortOrderMemoryMatch": {
            "orderBookLines": [{
                "price": "92533.5", "totalSize": "716", "userId": 600004701
            }, {
                "price": "92579.8", "totalSize": "195", "userId": 600004701
            }, {
                "price": "92617.5", "totalSize": "671", "userId": 600004701
            }, {
                "price": "92630.6", "totalSize": "228", "userId": 600004701
            }, {
                "price": "92649", "totalSize": "849", "userId": 600004701
            }, {
                "price": "92687.4", "totalSize": "823", "userId": 600004701
            }, {
                "price": "92688.6", "totalSize": "754", "userId": 600004701
            }, {
                "price": "92691.3", "totalSize": "474", "userId": 600004701
            }, {
                "price": "92691.8", "totalSize": "845", "userId": 600004701
            }, {
                "price": "92725.1", "totalSize": "483", "userId": 600004701
            }, {
                "price": "92753.7", "totalSize": "782", "userId": 600004701
            }, {
                "price": "92796.1", "totalSize": "4", "userId": 300615600005616
            }, {
                "price": "92799.4", "totalSize": "105", "userId": 600004701
            }, {
                "price": "92803.5", "totalSize": "301", "userId": 600004701
            }, {
                "price": "92810.4", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "92818", "totalSize": "849", "userId": 600004701
            }, {
                "price": "92825", "totalSize": "1", "userId": 300013600015725
            }, {
                "price": "92841.9", "totalSize": "1004", "userId": 600004701
            }, {
                "price": "92844.4", "totalSize": "973", "userId": 600004701
            }, {
                "price": "92848.3", "totalSize": "637", "userId": 600004701
            }, {
                "price": "92850", "totalSize": "1", "userId": 300013600015725
            }, {
                "price": "92850.2", "totalSize": "406", "userId": 600004701
            }, {
                "price": "92868.5", "totalSize": "969", "userId": 600004701
            }, {
                "price": "92871.9", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "92875", "totalSize": "1", "userId": 300013600015725
            }, {
                "price": "92882.7", "totalSize": "472", "userId": 600004701
            }, {
                "price": "92900", "totalSize": "1", "userId": 300013600015725
            }, {
                "price": "92906", "totalSize": "961", "userId": 600004701
            }, {
                "price": "92935.9", "totalSize": "997", "userId": 600004701
            }, {
                "price": "92970.5", "totalSize": "617", "userId": 600004701
            }, {
                "price": "93000", "totalSize": "136", "userId": 600004701
            }, {
                "price": "93003.7", "totalSize": "55", "userId": 600004701
            }, {
                "price": "93013.5", "totalSize": "653", "userId": 600004701
            }, {
                "price": "93036.5", "totalSize": "342", "userId": 600004701
            }, {
                "price": "93066.1", "totalSize": "199", "userId": 600004701
            }, {
                "price": "93081.2", "totalSize": "742", "userId": 600004701
            }, {
                "price": "93087.7", "totalSize": "964", "userId": 600004701
            }, {
                "price": "93098.3", "totalSize": "507", "userId": 600004701
            }, {
                "price": "93101", "totalSize": "176", "userId": 600004701
            }, {
                "price": "93104.8", "totalSize": "2", "userId": 600004701
            }, {
                "price": "93105.1", "totalSize": "770", "userId": 600004701
            }, {
                "price": "93140.9", "totalSize": "1033", "userId": 600004701
            }, {
                "price": "93157.7", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "93174.5", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "93176.2", "totalSize": "742", "userId": 600004701
            }, {
                "price": "93192.7", "totalSize": "4", "userId": 600004701
            }, {
                "price": "93215", "totalSize": "506", "userId": 600004701
            }, {
                "price": "93243", "totalSize": "962", "userId": 600004701
            }, {
                "price": "93257.8", "totalSize": "4", "userId": 300615600005616
            }, {
                "price": "93293.7", "totalSize": "1020", "userId": 600004701
            }, {
                "price": "93294", "totalSize": "399", "userId": 600004701
            }, {
                "price": "93329.5", "totalSize": "363", "userId": 600004701
            }, {
                "price": "93332.4", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "93344.9", "totalSize": "157", "userId": 600004701
            }, {
                "price": "93387.3", "totalSize": "276", "userId": 600004701
            }, {
                "price": "93421.9", "totalSize": "57", "userId": 600004701
            }, {
                "price": "93441", "totalSize": "247", "userId": 600004701
            }, {
                "price": "93443.4", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "93464", "totalSize": "896", "userId": 600004701
            }, {
                "price": "93498.1", "totalSize": "811", "userId": 600004701
            }, {
                "price": "93538.5", "totalSize": "1", "userId": 300004600010487
            }, {
                "price": "93540.7", "totalSize": "806", "userId": 600004701
            }, {
                "price": "93580.9", "totalSize": "574", "userId": 600004701
            }, {
                "price": "93634.9", "totalSize": "688", "userId": 600004701
            }, {
                "price": "93653.5", "totalSize": "792", "userId": 600004701
            }, {
                "price": "93661.8", "totalSize": "293", "userId": 600004701
            }, {
                "price": "93676.1", "totalSize": "137", "userId": 600004701
            }, {
                "price": "93688.8", "totalSize": "1058", "userId": 600004701
            }, {
                "price": "93719.5", "totalSize": "5", "userId": 300615600005616
            }, {
                "price": "93728.2", "totalSize": "753", "userId": 600004701
            }, {
                "price": "93729.1", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "93779.4", "totalSize": "257", "userId": 600004701
            }, {
                "price": "93808.9", "totalSize": "112", "userId": 600004701
            }, {
                "price": "93824.4", "totalSize": "572", "userId": 600004701
            }, {
                "price": "93857.8", "totalSize": "931", "userId": 600004701
            }, {
                "price": "93882.2", "totalSize": "914", "userId": 600004701
            }, {
                "price": "93887.9", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "93900.7", "totalSize": "343", "userId": 600004701
            }, {
                "price": "93902.6", "totalSize": "3", "userId": 300125600010326
            }, {
                "price": "93924.7", "totalSize": "571", "userId": 600004701
            }, {
                "price": "93941.5", "totalSize": "82", "userId": 600004701
            }, {
                "price": "93957.2", "totalSize": "488", "userId": 600004701
            }, {
                "price": "93959.5", "totalSize": "830", "userId": 600004701
            }, {
                "price": "94001.4", "totalSize": "111", "userId": 600004701
            }, {
                "price": "94014.8", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "94042.4", "totalSize": "600", "userId": 600004701
            }, {
                "price": "94117.2", "totalSize": "29", "userId": 600004701
            }, {
                "price": "94118.5", "totalSize": "724", "userId": 600004701
            }, {
                "price": "94121.2", "totalSize": "128", "userId": 600004701
            }, {
                "price": "94166.5", "totalSize": "1056", "userId": 600004701
            }, {
                "price": "94181.1", "totalSize": "5", "userId": 300615600005616
            }, {
                "price": "94209.6", "totalSize": "1019", "userId": 600004701
            }, {
                "price": "94222.2", "totalSize": "365", "userId": 600004701
            }, {
                "price": "94252.7", "totalSize": "419", "userId": 600004701
            }, {
                "price": "94266.7", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "94296.9", "totalSize": "202", "userId": 600004701
            }, {
                "price": "94300.5", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "94344", "totalSize": "549", "userId": 600004701
            }, {
                "price": "94429.9", "totalSize": "1018", "userId": 600004701
            }, {
                "price": "94443.5", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "94444.5", "totalSize": "899", "userId": 600004701
            }, {
                "price": "94446", "totalSize": "184", "userId": 600004701
            }, {
                "price": "94448.5", "totalSize": "282", "userId": 600004701
            }, {
                "price": "94459.1", "totalSize": "268", "userId": 600004701
            }, {
                "price": "94512.5", "totalSize": "894", "userId": 600004701
            }, {
                "price": "94550.4", "totalSize": "576", "userId": 600004701
            }, {
                "price": "94554.8", "totalSize": "6", "userId": 300522600005621
            }, {
                "price": "94555", "totalSize": "565", "userId": 600004701
            }, {
                "price": "94585.9", "totalSize": "9", "userId": 300606600005616
            }, {
                "price": "94586.2", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "94630.8", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "94642.8", "totalSize": "6", "userId": 300615600005616
            }, {
                "price": "94676.7", "totalSize": "6", "userId": 300076600016119
            }, {
                "price": "94683.8", "totalSize": "1", "userId": 300045600001142
            }, {
                "price": "94692.8", "totalSize": "858", "userId": 600004701
            }, {
                "price": "94705.4", "totalSize": "14", "userId": 300029600016073
            }, {
                "price": "94708.3", "totalSize": "2", "userId": 300541600005621
            }, {
                "price": "94713.1", "totalSize": "7", "userId": 300057600016119
            }, {
                "price": "94790.1", "totalSize": "7", "userId": 300066600016119
            }, {
                "price": "94799.2", "totalSize": "3", "userId": 300012600017611
            }, {
                "price": "94840.3", "totalSize": "7", "userId": 300083600016119
            }, {
                "price": "94845.5", "totalSize": "487", "userId": 600004701
            }, {
                "price": "94872", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "94991.2", "totalSize": "8", "userId": 300540600005621
            }, {
                "price": "94994.9", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "94999", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "95025.6", "totalSize": "1045", "userId": 600004701
            }, {
                "price": "95037.1", "totalSize": "407", "userId": 300014600001340
            }, {
                "price": "95038.5", "totalSize": "10", "userId": 300606600005616
            }, {
                "price": "95104.5", "totalSize": "6", "userId": 300615600005616
            }, {
                "price": "95129.7", "totalSize": "6", "userId": 300076600016119
            }, {
                "price": "95144.3", "totalSize": "1040", "userId": 600004701
            }, {
                "price": "95164.1", "totalSize": "7", "userId": 300057600016119
            }, {
                "price": "95239.4", "totalSize": "8", "userId": 300066600016119
            }, {
                "price": "95263.9", "totalSize": "4", "userId": 300012600017611
            }, {
                "price": "95283.6", "totalSize": "765", "userId": 600004701
            }, {
                "price": "95291.9", "totalSize": "7", "userId": 300083600016119
            }, {
                "price": "95319.4", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "95358.9", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "95399.7", "totalSize": "1007", "userId": 600004701
            }, {
                "price": "95443.6", "totalSize": "9", "userId": 300540600005621
            }, {
                "price": "95463.8", "totalSize": "74", "userId": 300024600000559
            }, {
                "price": "95491.1", "totalSize": "11", "userId": 300606600005616
            }, {
                "price": "95523.4", "totalSize": "666", "userId": 600004701
            }, {
                "price": "95554.6", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "95566.2", "totalSize": "7", "userId": 300615600005616
            }, {
                "price": "95582.7", "totalSize": "7", "userId": 300076600016119
            }, {
                "price": "95612.1", "totalSize": "1", "userId": 300045600001142
            }, {
                "price": "95615.1", "totalSize": "8", "userId": 300057600016119
            }, {
                "price": "95636.9", "totalSize": "721", "userId": 600004701
            }, {
                "price": "95688.6", "totalSize": "9", "userId": 300066600016119
            }, {
                "price": "95723", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "95728.6", "totalSize": "4", "userId": 300012600017611
            }, {
                "price": "95743.5", "totalSize": "8", "userId": 300083600016119
            }, {
                "price": "95790", "totalSize": "652", "userId": 600004701
            }, {
                "price": "95895.9", "totalSize": "10", "userId": 300540600005621
            }, {
                "price": "95932.1", "totalSize": "990", "userId": 600004701
            }, {
                "price": "95943.6", "totalSize": "12", "userId": 300606600005616
            }, {
                "price": "96000", "totalSize": "10", "userId": 600010166
            }, {
                "price": "96027.8", "totalSize": "7", "userId": 300615600005616
            }, {
                "price": "96035.7", "totalSize": "8", "userId": 300076600016119
            }, {
                "price": "96066.1", "totalSize": "9", "userId": 300057600016119
            }, {
                "price": "96087.1", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "96105.7", "totalSize": "712", "userId": 600004701
            }, {
                "price": "96110.1", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "96193.3", "totalSize": "5", "userId": 300012600017611
            }, {
                "price": "96195.1", "totalSize": "9", "userId": 300083600016119
            }, {
                "price": "96213.8", "totalSize": "917", "userId": 600004701
            }, {
                "price": "96348.3", "totalSize": "11", "userId": 300540600005621
            }, {
                "price": "96371.1", "totalSize": "1058", "userId": 600004701
            }, {
                "price": "96396.2", "totalSize": "13", "userId": 300606600005616
            }, {
                "price": "96451.2", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "96471.5", "totalSize": "972", "userId": 600004701
            }, {
                "price": "96488.7", "totalSize": "9", "userId": 300076600016119
            }, {
                "price": "96489.5", "totalSize": "8", "userId": 300615600005616
            }, {
                "price": "96540.4", "totalSize": "1", "userId": 300045600001142
            }, {
                "price": "96570.7", "totalSize": "678", "userId": 600004701
            }, {
                "price": "96658", "totalSize": "5", "userId": 300012600017611
            }, {
                "price": "96665.7", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "96692.1", "totalSize": "1007", "userId": 600004701
            }, {
                "price": "96797.4", "totalSize": "978", "userId": 600004701
            }, {
                "price": "96815.2", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "96951.2", "totalSize": "9", "userId": 300615600005616
            }, {
                "price": "96972.3", "totalSize": "724", "userId": 600004701
            }, {
                "price": "97092.7", "totalSize": "872", "userId": 600004701
            }, {
                "price": "97122.7", "totalSize": "6", "userId": 300012600017611
            }, {
                "price": "97179.3", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "97221.2", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "97267.7", "totalSize": "511", "userId": 600004701
            }, {
                "price": "97412.6", "totalSize": "880", "userId": 600004701
            }, {
                "price": "97412.8", "totalSize": "10", "userId": 300615600005616
            }, {
                "price": "97468.7", "totalSize": "1", "userId": 300045600001142
            }, {
                "price": "97474", "totalSize": "407", "userId": 300014600001340
            }, {
                "price": "97535.7", "totalSize": "1017", "userId": 600004701
            }, {
                "price": "97543.4", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "97587.4", "totalSize": "6", "userId": 300012600017611
            }, {
                "price": "97701.4", "totalSize": "1069", "userId": 600004701
            }, {
                "price": "97776.8", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "97874.5", "totalSize": "11", "userId": 300615600005616
            }, {
                "price": "97876.1", "totalSize": "470", "userId": 600004701
            }, {
                "price": "97907.5", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "98001", "totalSize": "50", "userId": 600017575
            }, {
                "price": "98034.4", "totalSize": "828", "userId": 600004701
            }, {
                "price": "98052.1", "totalSize": "7", "userId": 300012600017611
            }, {
                "price": "98153.8", "totalSize": "981", "userId": 600004701
            }, {
                "price": "98271.6", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "98272.9", "totalSize": "478", "userId": 600004701
            }, {
                "price": "98332.3", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "98336.2", "totalSize": "12", "userId": 300615600005616
            }, {
                "price": "98389.4", "totalSize": "603", "userId": 600004701
            }, {
                "price": "98516.8", "totalSize": "8", "userId": 300012600017611
            }, {
                "price": "98547.8", "totalSize": "1008", "userId": 600004701
            }, {
                "price": "98635.6", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "98662.5", "totalSize": "955", "userId": 600004701
            }, {
                "price": "98838.1", "totalSize": "989", "userId": 600004701
            }, {
                "price": "98887.9", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "98969.8", "totalSize": "887", "userId": 600004701
            }, {
                "price": "98981.5", "totalSize": "8", "userId": 300012600017611
            }, {
                "price": "98999.7", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "99089.7", "totalSize": "546", "userId": 600004701
            }, {
                "price": "99222.3", "totalSize": "803", "userId": 600004701
            }, {
                "price": "99317.9", "totalSize": "765", "userId": 600004701
            }, {
                "price": "99363.8", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "99427.8", "totalSize": "488", "userId": 600004701
            }, {
                "price": "99443.4", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "99529.5", "totalSize": "643", "userId": 600004701
            }, {
                "price": "99669.8", "totalSize": "974", "userId": 600004701
            }, {
                "price": "99727.9", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "99764.9", "totalSize": "896", "userId": 600004701
            }, {
                "price": "99868.9", "totalSize": "510", "userId": 600004701
            }, {
                "price": "99910.8", "totalSize": "407", "userId": 300014600001340
            }, {
                "price": "99999", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "100000", "totalSize": "4", "userId": 700005600015215
            }, {
                "price": "100043.9", "totalSize": "577", "userId": 600004701
            }, {
                "price": "100092", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "100175.7", "totalSize": "543", "userId": 600004701
            }, {
                "price": "100286.1", "totalSize": "656", "userId": 600004701
            }, {
                "price": "100438.6", "totalSize": "599", "userId": 600004701
            }, {
                "price": "100551.1", "totalSize": "911", "userId": 600004701
            }, {
                "price": "100567.1", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "101034.2", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "101501.3", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "101968.4", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "102347.7", "totalSize": "407", "userId": 300014600001340
            }, {
                "price": "102435.6", "totalSize": "36136", "userId": 300070600004107
            }, {
                "price": "102902.7", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "103369.8", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "103836.9", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "104304", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "104771.2", "totalSize": "36136", "userId": 300070600004107
            }, {
                "price": "105000", "totalSize": "74", "userId": 300118600010326
            }, {
                "price": "105238.3", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "105705.4", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "106172.5", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "106639.6", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "107106.8", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "107573.9", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "108041", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "108508.1", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "108975.2", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "109442.4", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "109642.6", "totalSize": "1", "userId": 300039600006233
            }, {
                "price": "109909.5", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "110000", "totalSize": "74", "userId": 300118600010326
            }, {
                "price": "110376.6", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "110843.7", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "111310.8", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "111778", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "112245.1", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "112712.2", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "113179.3", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "113646.4", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "114113.6", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "114580.7", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "115047.8", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "115514.9", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "115982", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "116449.2", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "116916.3", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "117383.4", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "117850.5", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "118317.6", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "118784.8", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "119251.9", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "119719", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "120000", "totalSize": "1", "userId": 300019600016073
            }, {
                "price": "120186.1", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "120653.2", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "121120.4", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "121587.5", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "122054.6", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "122521.7", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "122988.8", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "123456", "totalSize": "2", "userId": 300070600004107
            }, {
                "price": "138001", "totalSize": "10", "userId": 600017546
            }]
        }, "dealOrders": [{
            "baseSize": 0.973,
            "contractType": 1,
            "createdDate": 1765357447741,
            "currentPiece": 973,
            "dealTime": 1765357447750000082,
            "direction": "long",
            "feeRate": 0,
            "frozenFee": 0,
            "id": 54635521192807516,
            "instrument": "BTC",
            "leverage": 1,
            "liquidateBy": "cancel",
            "makerFee": 0,
            "margin": 89820.06250000000,
            "needUpdateOrder": false,
            "orderPrice": 92312.5,
            "orderStatus": "cancelAll",
            "originalType": "plan",
            "posType": "plan",
            "positionModel": 1,
            "processStatus": 0,
            "quantity": 973,
            "quantityUnit": 1,
            "relationId": 54635521192807376,
            "singleDealAmount": 0,
            "singleDealFee": 0,
            "singleDealPrice": 0,
            "source": "web",
            "status": "open",
            "takerFee": 0,
            "takerMaker": 1,
            "thirdOrderId": "b2ea0d0724344f3b8ec4b9d8df9c91db",
            "totalPiece": 973,
            "type": "cancel",
            "updatedDate": 1765357447741,
            "userId": 600004701
        }, {
            "baseSize": 0.027,
            "contractType": 1,
            "createdDate": 1765357447741,
            "currentPiece": 27,
            "dealTime": 1765357447750000044,
            "direction": "short",
            "feeRate": 0,
            "frozenFee": 0,
            "id": 54635521192807517,
            "instrument": "BTC",
            "leverage": 1,
            "liquidateBy": "cancel",
            "makerFee": 0,
            "margin": 2504.56050000,
            "needUpdateOrder": false,
            "orderPrice": 92761.5,
            "orderStatus": "cancelAll",
            "originalType": "plan",
            "posType": "plan",
            "positionModel": 1,
            "processStatus": 0,
            "quantity": 27,
            "quantityUnit": 1,
            "relationId": 54635521192807373,
            "singleDealAmount": 0,
            "singleDealFee": 0,
            "singleDealPrice": 0,
            "source": "web",
            "status": "open",
            "takerFee": 0,
            "takerMaker": 1,
            "thirdOrderId": "96ec42bff4864c8290b0be090745668a",
            "totalPiece": 27,
            "type": "cancel",
            "updatedDate": 1765357447741,
            "userId": 600004701
        }, {
            "baseSize": 0.143,
            "contractType": 1,
            "createdDate": 1765357447741,
            "currentPiece": 143,
            "dealTime": 1765357447750000048,
            "direction": "long",
            "feeRate": 0,
            "frozenFee": 0,
            "id": 54635521192807518,
            "instrument": "BTC",
            "leverage": 1,
            "liquidateBy": "cancel",
            "makerFee": 0,
            "margin": 13181.74000000000,
            "needUpdateOrder": false,
            "orderPrice": 92180.0,
            "orderStatus": "cancelAll",
            "originalType": "plan",
            "posType": "plan",
            "positionModel": 1,
            "processStatus": 0,
            "quantity": 143,
            "quantityUnit": 1,
            "relationId": 54635521192807479,
            "singleDealAmount": 0,
            "singleDealFee": 0,
            "singleDealPrice": 0,
            "source": "web",
            "status": "open",
            "takerFee": 0,
            "takerMaker": 1,
            "thirdOrderId": "a28eae7369b047cfa07a7fd117994e79",
            "totalPiece": 143,
            "type": "cancel",
            "updatedDate": 1765357447741,
            "userId": 600004701
        }, {
            "baseSize": 0.890,
            "contractType": 1,
            "createdDate": 1765357447741,
            "currentPiece": 890,
            "dealTime": 1765357447750000041,
            "direction": "short",
            "feeRate": 0,
            "frozenFee": 0,
            "id": 54635521192807519,
            "instrument": "BTC",
            "leverage": 1,
            "liquidateBy": "cancel",
            "makerFee": 0,
            "margin": 82492.58700000,
            "needUpdateOrder": false,
            "orderPrice": 92688.3,
            "orderStatus": "cancelAll",
            "originalType": "plan",
            "posType": "plan",
            "positionModel": 1,
            "processStatus": 0,
            "quantity": 890,
            "quantityUnit": 1,
            "relationId": 54635521192807477,
            "singleDealAmount": 0,
            "singleDealFee": 0,
            "singleDealPrice": 0,
            "source": "web",
            "status": "open",
            "takerFee": 0,
            "takerMaker": 1,
            "thirdOrderId": "d6161738e82a4ec3b6038a7fabe20100",
            "totalPiece": 890,
            "type": "cancel",
            "updatedDate": 1765357447741,
            "userId": 600004701
        }, {
            "baseSize": 0.501,
            "contractType": 1,
            "createdDate": 1765357447741,
            "currentPiece": 501,
            "dealTime": 1765357447750000070,
            "direction": "long",
            "feeRate": 0,
            "frozenFee": 0,
            "id": 54635521192807520,
            "instrument": "BTC",
            "leverage": 1,
            "liquidateBy": "cancel",
            "makerFee": 0,
            "margin": 46349.96490000000,
            "needUpdateOrder": false,
            "orderPrice": 92514.9,
            "orderStatus": "cancelAll",
            "originalType": "plan",
            "posType": "plan",
            "positionModel": 1,
            "processStatus": 0,
            "quantity": 501,
            "quantityUnit": 1,
            "relationId": 54635521192807475,
            "singleDealAmount": 0,
            "singleDealFee": 0,
            "singleDealPrice": 0,
            "source": "web",
            "status": "open",
            "takerFee": 0,
            "takerMaker": 1,
            "thirdOrderId": "3cccb573fa77408dab5c98e822dab0d2",
            "totalPiece": 501,
            "type": "cancel",
            "updatedDate": 1765357447741,
            "userId": 600004701
        }, {
            "baseSize": 0.847,
            "contractType": 1,
            "createdDate": 1765357447741,
            "currentPiece": 847,
            "dealTime": 1765357447750000058,
            "direction": "short",
            "feeRate": 0,
            "frozenFee": 0,
            "id": 54635521192807521,
            "instrument": "BTC",
            "leverage": 1,
            "liquidateBy": "cancel",
            "makerFee": 0,
            "margin": 78375.87450000,
            "needUpdateOrder": false,
            "orderPrice": 92533.5,
            "orderStatus": "cancelAll",
            "originalType": "plan",
            "posType": "plan",
            "positionModel": 1,
            "processStatus": 0,
            "quantity": 847,
            "quantityUnit": 1,
            "relationId": 54635521192807480,
            "singleDealAmount": 0,
            "singleDealFee": 0,
            "singleDealPrice": 0,
            "source": "web",
            "status": "open",
            "takerFee": 0,
            "takerMaker": 1,
            "thirdOrderId": "4a948a4720e3465c8432fb926f580321",
            "totalPiece": 847,
            "type": "cancel",
            "updatedDate": 1765357447741,
            "userId": 600004701
        }], "longOrderMemoryMatch": {
            "orderBookLines": [{
                "price": "92514.9", "totalSize": "752", "userId": 600004701
            }, {
                "price": "92500.3", "totalSize": "1387", "userId": 600004701
            }, {
                "price": "92485.7", "totalSize": "1329", "userId": 600004701
            }, {
                "price": "92483", "totalSize": "116", "userId": 600004701
            }, {
                "price": "92479", "totalSize": "969", "userId": 600004701
            }, {
                "price": "92475", "totalSize": "193", "userId": 600004701
            }, {
                "price": "92443.9", "totalSize": "661", "userId": 600004701
            }, {
                "price": "92429", "totalSize": "49", "userId": 600004701
            }, {
                "price": "92398.5", "totalSize": "469", "userId": 600004701
            }, {
                "price": "92396.4", "totalSize": "448", "userId": 600004701
            }, {
                "price": "92376.7", "totalSize": "372", "userId": 600004701
            }, {
                "price": "92371.8", "totalSize": "908", "userId": 600004701
            }, {
                "price": "92354.7", "totalSize": "815", "userId": 600004701
            }, {
                "price": "92353.2", "totalSize": "611", "userId": 600004701
            }, {
                "price": "92351.1", "totalSize": "954", "userId": 600004701
            }, {
                "price": "92340.8", "totalSize": "396", "userId": 600004701
            }, {
                "price": "92336.6", "totalSize": "1883", "userId": 600004701
            }, {
                "price": "92334.5", "totalSize": "322", "userId": 600004701
            }, {
                "price": "92329.2", "totalSize": "475", "userId": 600004701
            }, {
                "price": "92321.2", "totalSize": "290", "userId": 600004701
            }, {
                "price": "92317.8", "totalSize": "691", "userId": 600004701
            }, {
                "price": "92316.7", "totalSize": "777", "userId": 600004701
            }, {
                "price": "92300.5", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "92264.3", "totalSize": "963", "userId": 600004701
            }, {
                "price": "92248.4", "totalSize": "406", "userId": 600004701
            }, {
                "price": "92245.7", "totalSize": "2382", "userId": 600004701
            }, {
                "price": "92223.9", "totalSize": "190", "userId": 600004701
            }, {
                "price": "92221.3", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "92186", "totalSize": "1066", "userId": 600004701
            }, {
                "price": "92172.7", "totalSize": "764", "userId": 600004701
            }, {
                "price": "92157.7", "totalSize": "582", "userId": 600004701
            }, {
                "price": "92143.4", "totalSize": "416", "userId": 600004701
            }, {
                "price": "92092.8", "totalSize": "55", "userId": 600004701
            }, {
                "price": "92082.2", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "92052.3", "totalSize": "557", "userId": 600004701
            }, {
                "price": "92045.3", "totalSize": "659", "userId": 600004701
            }, {
                "price": "92040.8", "totalSize": "148", "userId": 600004701
            }, {
                "price": "92014.8", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "92007.2", "totalSize": "73", "userId": 600004701
            }, {
                "price": "91994.6", "totalSize": "507", "userId": 600004701
            }, {
                "price": "91977.4", "totalSize": "906", "userId": 600004701
            }, {
                "price": "91965.3", "totalSize": "519", "userId": 600004701
            }, {
                "price": "91949.9", "totalSize": "887", "userId": 600004701
            }, {
                "price": "91936", "totalSize": "884", "userId": 600004701
            }, {
                "price": "91913.5", "totalSize": "748", "userId": 600004701
            }, {
                "price": "91905.6", "totalSize": "674", "userId": 600004701
            }, {
                "price": "91899", "totalSize": "1", "userId": 300045600001142
            }, {
                "price": "91878", "totalSize": "582", "userId": 600004701
            }, {
                "price": "91874.9", "totalSize": "176", "userId": 600004701
            }, {
                "price": "91841.8", "totalSize": "5", "userId": 300024600000559
            }, {
                "price": "91835", "totalSize": "576", "userId": 600004701
            }, {
                "price": "91808.5", "totalSize": "830", "userId": 600004701
            }, {
                "price": "91791.6", "totalSize": "84", "userId": 600004701
            }, {
                "price": "91759.2", "totalSize": "821", "userId": 600004701
            }, {
                "price": "91752.1", "totalSize": "414", "userId": 600004701
            }, {
                "price": "91732.3", "totalSize": "720", "userId": 600004701
            }, {
                "price": "91729.7", "totalSize": "761", "userId": 600004701
            }, {
                "price": "91729.1", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "91721.3", "totalSize": "746", "userId": 600004701
            }, {
                "price": "91718.2", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "91696.2", "totalSize": "396", "userId": 600004701
            }, {
                "price": "91685.8", "totalSize": "571", "userId": 600004701
            }, {
                "price": "91669.1", "totalSize": "561", "userId": 600004701
            }, {
                "price": "91665.7", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "91649.8", "totalSize": "547", "userId": 600004701
            }, {
                "price": "91648.3", "totalSize": "766", "userId": 600004701
            }, {
                "price": "91616.1", "totalSize": "75", "userId": 600004701
            }, {
                "price": "91583.5", "totalSize": "43", "userId": 600004701
            }, {
                "price": "91563.8", "totalSize": "861", "userId": 600004701
            }, {
                "price": "91549.9", "totalSize": "6", "userId": 300024600000559
            }, {
                "price": "91539", "totalSize": "332", "userId": 600004701
            }, {
                "price": "91514.4", "totalSize": "303", "userId": 600004701
            }, {
                "price": "91508.6", "totalSize": "505", "userId": 600004701
            }, {
                "price": "91473.1", "totalSize": "594", "userId": 600004701
            }, {
                "price": "91452.7", "totalSize": "492", "userId": 600004701
            }, {
                "price": "91443.4", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "91439.1", "totalSize": "230", "userId": 600004701
            }, {
                "price": "91437.5", "totalSize": "1000", "userId": 600004701
            }, {
                "price": "91395.8", "totalSize": "825", "userId": 600004701
            }, {
                "price": "91383.9", "totalSize": "164", "userId": 600004701
            }, {
                "price": "91359.8", "totalSize": "551", "userId": 600004701
            }, {
                "price": "91354.1", "totalSize": "3", "userId": 300125600010326
            }, {
                "price": "91322.4", "totalSize": "452", "userId": 600004701
            }, {
                "price": "91267.6", "totalSize": "331", "userId": 600004701
            }, {
                "price": "91257.9", "totalSize": "6", "userId": 300024600000559
            }, {
                "price": "91228", "totalSize": "99", "userId": 600004701
            }, {
                "price": "91174.5", "totalSize": "197", "userId": 600004701
            }, {
                "price": "91157.7", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "91144.8", "totalSize": "1029", "userId": 600004701
            }, {
                "price": "91134.7", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "91110.2", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "91030.3", "totalSize": "302", "userId": 600004701
            }, {
                "price": "91027.7", "totalSize": "1084", "userId": 600004701
            }, {
                "price": "91021.5", "totalSize": "225", "userId": 600004701
            }, {
                "price": "90990", "totalSize": "3", "userId": 300125600010326
            }, {
                "price": "90966", "totalSize": "7", "userId": 300024600000559
            }, {
                "price": "90871.9", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "90847.7", "totalSize": "512", "userId": 600004701
            }, {
                "price": "90785", "totalSize": "306", "userId": 600004701
            }, {
                "price": "90748.4", "totalSize": "668", "userId": 600004701
            }, {
                "price": "90731.1", "totalSize": "842", "userId": 600004701
            }, {
                "price": "90716.5", "totalSize": "609", "userId": 600004701
            }, {
                "price": "90692.5", "totalSize": "547", "userId": 600004701
            }, {
                "price": "90681.3", "totalSize": "287", "userId": 600004701
            }, {
                "price": "90674", "totalSize": "8", "userId": 300024600000559
            }, {
                "price": "90640.3", "totalSize": "1053", "userId": 600004701
            }, {
                "price": "90625.9", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "90586.2", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "90577.8", "totalSize": "176", "userId": 600004701
            }, {
                "price": "90554.6", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "90523.9", "totalSize": "438", "userId": 600004701
            }, {
                "price": "90494.7", "totalSize": "905", "userId": 600004701
            }, {
                "price": "90485.5", "totalSize": "351", "userId": 600004701
            }, {
                "price": "90382.1", "totalSize": "9", "userId": 300024600000559
            }, {
                "price": "90372.6", "totalSize": "622", "userId": 600004701
            }, {
                "price": "90302.2", "totalSize": "910", "userId": 600004701
            }, {
                "price": "90300.5", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "90261.9", "totalSize": "3", "userId": 300016600001340
            }, {
                "price": "90259.1", "totalSize": "1084", "userId": 600004701
            }, {
                "price": "90203.6", "totalSize": "491", "userId": 600004701
            }, {
                "price": "90184", "totalSize": "635", "userId": 600004701
            }, {
                "price": "90161.9", "totalSize": "262", "userId": 600004701
            }, {
                "price": "90160.7", "totalSize": "913", "userId": 600004701
            }, {
                "price": "90122.6", "totalSize": "366", "userId": 600004701
            }, {
                "price": "90117.4", "totalSize": "668", "userId": 600004701
            }, {
                "price": "90117.1", "totalSize": "881", "userId": 600004701
            }, {
                "price": "90090.1", "totalSize": "10", "userId": 300024600000559
            }, {
                "price": "90089", "totalSize": "315", "userId": 600004701
            }, {
                "price": "90024.8", "totalSize": "563", "userId": 600004701
            }, {
                "price": "90021", "totalSize": "994", "userId": 600004701
            }, {
                "price": "90014.8", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "90000", "totalSize": "10", "userId": 600015678
            }, {
                "price": "89999.1", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "89976", "totalSize": "285", "userId": 600004701
            }, {
                "price": "89909.1", "totalSize": "963", "userId": 600004701
            }, {
                "price": "89903.2", "totalSize": "771", "userId": 600004701
            }, {
                "price": "89880.5", "totalSize": "1521", "userId": 600004701
            }, {
                "price": "89841.6", "totalSize": "125", "userId": 600004701
            }, {
                "price": "89838.3", "totalSize": "941", "userId": 600004701
            }, {
                "price": "89821.1", "totalSize": "1056", "userId": 600004701
            }, {
                "price": "89818.1", "totalSize": "513", "userId": 600004701
            }, {
                "price": "89816.2", "totalSize": "1029", "userId": 600004701
            }, {
                "price": "89798.2", "totalSize": "11", "userId": 300024600000559
            }, {
                "price": "89785.6", "totalSize": "38", "userId": 600004701
            }, {
                "price": "89775.2", "totalSize": "356", "userId": 600004701
            }, {
                "price": "89773.2", "totalSize": "998", "userId": 600004701
            }, {
                "price": "89771.1", "totalSize": "777", "userId": 600004701
            }, {
                "price": "89735.1", "totalSize": "557", "userId": 600004701
            }, {
                "price": "89732.8", "totalSize": "333", "userId": 600004701
            }, {
                "price": "89729.1", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "89696.1", "totalSize": "1141", "userId": 600004701
            }, {
                "price": "89637.4", "totalSize": "1", "userId": 300541600005621
            }, {
                "price": "89583.9", "totalSize": "602", "userId": 600004701
            }, {
                "price": "89564.8", "totalSize": "10", "userId": 300602600005616
            }, {
                "price": "89506.2", "totalSize": "12", "userId": 300024600000559
            }, {
                "price": "89484.5", "totalSize": "693", "userId": 600004701
            }, {
                "price": "89476.4", "totalSize": "674", "userId": 600004701
            }, {
                "price": "89443.5", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "89443.4", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "89356.9", "totalSize": "2", "userId": 300541600005621
            }, {
                "price": "89336.9", "totalSize": "860", "userId": 600004701
            }, {
                "price": "89274.8", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "89225.7", "totalSize": "897", "userId": 600004701
            }, {
                "price": "89214.3", "totalSize": "13", "userId": 300024600000559
            }, {
                "price": "89193.7", "totalSize": "1108", "userId": 600004701
            }, {
                "price": "89157.7", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "89154.9", "totalSize": "126", "userId": 600004701
            }, {
                "price": "89143.2", "totalSize": "3", "userId": 300029600016073
            }, {
                "price": "89086.3", "totalSize": "1043", "userId": 600004701
            }, {
                "price": "89076.4", "totalSize": "2", "userId": 300541600005621
            }, {
                "price": "89052.2", "totalSize": "385", "userId": 600004701
            }, {
                "price": "88973.5", "totalSize": "734", "userId": 600004701
            }, {
                "price": "88922.3", "totalSize": "15", "userId": 300024600000559
            }, {
                "price": "88888", "totalSize": "1", "userId": 300616600005616
            }, {
                "price": "88871.9", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "88864.6", "totalSize": "1082", "userId": 600004701
            }, {
                "price": "88862.8", "totalSize": "949", "userId": 600004701
            }, {
                "price": "88859.8", "totalSize": "3", "userId": 300029600016073
            }, {
                "price": "88850", "totalSize": "851", "userId": 600004701
            }, {
                "price": "88795.9", "totalSize": "2", "userId": 300541600005621
            }, {
                "price": "88761.8", "totalSize": "368", "userId": 600004701
            }, {
                "price": "88740.1", "totalSize": "727", "userId": 600004701
            }, {
                "price": "88737.9", "totalSize": "377", "userId": 600004701
            }, {
                "price": "88716.3", "totalSize": "797", "userId": 600004701
            }, {
                "price": "88674.1", "totalSize": "1", "userId": 600004701
            }, {
                "price": "88659.1", "totalSize": "797", "userId": 600004701
            }, {
                "price": "88630.4", "totalSize": "16", "userId": 300024600000559
            }, {
                "price": "88606.9", "totalSize": "574", "userId": 600004701
            }, {
                "price": "88587", "totalSize": "1184", "userId": 600004701
            }, {
                "price": "88586.2", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "88576.4", "totalSize": "4", "userId": 300029600016073
            }, {
                "price": "88538.2", "totalSize": "180", "userId": 600004701
            }, {
                "price": "88515.5", "totalSize": "2", "userId": 300541600005621
            }, {
                "price": "88439.9", "totalSize": "1016", "userId": 600004701
            }, {
                "price": "88338.4", "totalSize": "18", "userId": 300024600000559
            }, {
                "price": "88300.5", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "88298.3", "totalSize": "1025", "userId": 600004701
            }, {
                "price": "88293.1", "totalSize": "4", "userId": 300029600016073
            }, {
                "price": "88235", "totalSize": "3", "userId": 300541600005621
            }, {
                "price": "88185.9", "totalSize": "1", "userId": 300045600001142
            }, {
                "price": "88159.9", "totalSize": "949", "userId": 600004701
            }, {
                "price": "88156.8", "totalSize": "688", "userId": 600004701
            }, {
                "price": "88128.3", "totalSize": "1164", "userId": 600004701
            }, {
                "price": "88067.5", "totalSize": "485", "userId": 600004701
            }, {
                "price": "88046.5", "totalSize": "20", "userId": 300024600000559
            }, {
                "price": "88029", "totalSize": "693", "userId": 600004701
            }, {
                "price": "88017.7", "totalSize": "599", "userId": 600004701
            }, {
                "price": "88014.8", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "88009.7", "totalSize": "5", "userId": 300029600016073
            }, {
                "price": "88000", "totalSize": "2", "userId": 300007600000977
            }, {
                "price": "87954.5", "totalSize": "3", "userId": 300541600005621
            }, {
                "price": "87948.8", "totalSize": "836", "userId": 600004701
            }, {
                "price": "87902.1", "totalSize": "955", "userId": 600004701
            }, {
                "price": "87891.2", "totalSize": "840", "userId": 600004701
            }, {
                "price": "87779.4", "totalSize": "225", "userId": 600004701
            }, {
                "price": "87754.5", "totalSize": "22", "userId": 300024600000559
            }, {
                "price": "87734.6", "totalSize": "833", "userId": 600004701
            }, {
                "price": "87729.1", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "87726.3", "totalSize": "5", "userId": 300029600016073
            }, {
                "price": "87688.1", "totalSize": "598", "userId": 600004701
            }, {
                "price": "87674", "totalSize": "3", "userId": 300541600005621
            }, {
                "price": "87618.7", "totalSize": "857", "userId": 600004701
            }, {
                "price": "87462.6", "totalSize": "24", "userId": 300024600000559
            }, {
                "price": "87456.3", "totalSize": "778", "userId": 600004701
            }, {
                "price": "87443.4", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "87442.9", "totalSize": "6", "userId": 300029600016073
            }, {
                "price": "87417.9", "totalSize": "1088", "userId": 600004701
            }, {
                "price": "87414.9", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "87393.5", "totalSize": "4", "userId": 300541600005621
            }, {
                "price": "87366", "totalSize": "861", "userId": 600004701
            }, {
                "price": "87283", "totalSize": "581", "userId": 600004701
            }, {
                "price": "87170.6", "totalSize": "27", "userId": 300024600000559
            }, {
                "price": "87159.6", "totalSize": "7", "userId": 300029600016073
            }, {
                "price": "87157.7", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "87139.3", "totalSize": "899", "userId": 600004701
            }, {
                "price": "87113", "totalSize": "4", "userId": 300541600005621
            }, {
                "price": "87105.3", "totalSize": "1083", "userId": 600004701
            }, {
                "price": "87091.3", "totalSize": "557", "userId": 600004701
            }, {
                "price": "87040.1", "totalSize": "587", "userId": 600004701
            }, {
                "price": "87007.3", "totalSize": "239", "userId": 600004701
            }, {
                "price": "86996.3", "totalSize": "806", "userId": 600004701
            }, {
                "price": "86883.8", "totalSize": "1002", "userId": 600004701
            }, {
                "price": "86878.7", "totalSize": "29", "userId": 300024600000559
            }, {
                "price": "86871.9", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "86832.6", "totalSize": "5", "userId": 300541600005621
            }, {
                "price": "86781.4", "totalSize": "929", "userId": 600004701
            }, {
                "price": "86725.8", "totalSize": "268", "userId": 600004701
            }, {
                "price": "86708.4", "totalSize": "749", "userId": 600004701
            }, {
                "price": "86623.7", "totalSize": "437", "userId": 600004701
            }, {
                "price": "86611.4", "totalSize": "637", "userId": 600004701
            }, {
                "price": "86587.2", "totalSize": "307", "userId": 600004701
            }, {
                "price": "86586.7", "totalSize": "33", "userId": 300024600000559
            }, {
                "price": "86586.2", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "86552.1", "totalSize": "5", "userId": 300541600005621
            }, {
                "price": "86550", "totalSize": "799", "userId": 600004701
            }, {
                "price": "86549.3", "totalSize": "551", "userId": 600004701
            }, {
                "price": "86541.6", "totalSize": "1004", "userId": 600004701
            }, {
                "price": "86538.3", "totalSize": "646", "userId": 600004701
            }, {
                "price": "86520.9", "totalSize": "662", "userId": 600004701
            }, {
                "price": "86506.4", "totalSize": "1317", "userId": 600004701
            }, {
                "price": "86442.8", "totalSize": "985", "userId": 600004701
            }, {
                "price": "86425.2", "totalSize": "909", "userId": 600004701
            }, {
                "price": "86411.5", "totalSize": "648", "userId": 600004701
            }, {
                "price": "86405.9", "totalSize": "1081", "userId": 600004701
            }, {
                "price": "86365.7", "totalSize": "429", "userId": 600004701
            }, {
                "price": "86358.7", "totalSize": "1097", "userId": 600004701
            }, {
                "price": "86352", "totalSize": "891", "userId": 600004701
            }, {
                "price": "86345.6", "totalSize": "93", "userId": 600004701
            }, {
                "price": "86338.4", "totalSize": "360", "userId": 600004701
            }, {
                "price": "86307.4", "totalSize": "2447", "userId": 600004701
            }, {
                "price": "86304.3", "totalSize": "636", "userId": 600004701
            }, {
                "price": "86300.5", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "86294.8", "totalSize": "36", "userId": 300024600000559
            }, {
                "price": "86271.6", "totalSize": "6", "userId": 300541600005621
            }, {
                "price": "86229.1", "totalSize": "776", "userId": 600004701
            }, {
                "price": "86223.9", "totalSize": "201", "userId": 600004701
            }, {
                "price": "86191.3", "totalSize": "68", "userId": 600004701
            }, {
                "price": "86191.1", "totalSize": "565", "userId": 600004701
            }, {
                "price": "86181.3", "totalSize": "937", "userId": 600004701
            }, {
                "price": "86180.1", "totalSize": "723", "userId": 600004701
            }, {
                "price": "86171.3", "totalSize": "906", "userId": 600004701
            }, {
                "price": "86165.4", "totalSize": "840", "userId": 600004701
            }, {
                "price": "86164.8", "totalSize": "623", "userId": 600004701
            }, {
                "price": "86161.1", "totalSize": "1148", "userId": 600004701
            }, {
                "price": "86143.1", "totalSize": "1059", "userId": 600004701
            }, {
                "price": "86081.1", "totalSize": "809", "userId": 600004701
            }, {
                "price": "86014.8", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "86002.8", "totalSize": "40", "userId": 300024600000559
            }, {
                "price": "86000", "totalSize": "10", "userId": 600006437
            }, {
                "price": "85984.1", "totalSize": "968", "userId": 600004701
            }, {
                "price": "85854.9", "totalSize": "835", "userId": 600004701
            }, {
                "price": "85740.2", "totalSize": "523", "userId": 600004701
            }, {
                "price": "85729.1", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "85710.9", "totalSize": "44", "userId": 300024600000559
            }, {
                "price": "85687.8", "totalSize": "614", "userId": 600004701
            }, {
                "price": "85555", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "85552.9", "totalSize": "533", "userId": 600004701
            }, {
                "price": "85521.7", "totalSize": "622", "userId": 600004701
            }, {
                "price": "85443.4", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "85420.2", "totalSize": "568", "userId": 600004701
            }, {
                "price": "85418.9", "totalSize": "49", "userId": 300024600000559
            }, {
                "price": "85380.2", "totalSize": "578", "userId": 600004701
            }, {
                "price": "85317.6", "totalSize": "597", "userId": 600004701
            }, {
                "price": "85173.8", "totalSize": "634", "userId": 600004701
            }, {
                "price": "85157.7", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "85127", "totalSize": "54", "userId": 300024600000559
            }, {
                "price": "85055.6", "totalSize": "1146", "userId": 600004701
            }, {
                "price": "85000", "totalSize": "10", "userId": 600015678
            }, {
                "price": "84966.2", "totalSize": "138", "userId": 600004701
            }, {
                "price": "84944.5", "totalSize": "1066", "userId": 600004701
            }, {
                "price": "84938.5", "totalSize": "537", "userId": 600004701
            }, {
                "price": "84881.1", "totalSize": "1001", "userId": 600004701
            }, {
                "price": "84874.9", "totalSize": "849", "userId": 600004701
            }, {
                "price": "84872", "totalSize": "1", "userId": 300023600000559
            }, {
                "price": "84835", "totalSize": "59", "userId": 300024600000559
            }, {
                "price": "84768.4", "totalSize": "911", "userId": 600004701
            }, {
                "price": "84687.8", "totalSize": "118", "userId": 600004701
            }, {
                "price": "84676.8", "totalSize": "310", "userId": 600004701
            }, {
                "price": "84668.8", "totalSize": "919", "userId": 600004701
            }, {
                "price": "84585.8", "totalSize": "730", "userId": 600004701
            }, {
                "price": "84575.8", "totalSize": "749", "userId": 600004701
            }, {
                "price": "84554.7", "totalSize": "917", "userId": 600004701
            }, {
                "price": "84543.1", "totalSize": "66", "userId": 300024600000559
            }, {
                "price": "84510.3", "totalSize": "909", "userId": 600004701
            }, {
                "price": "84500.3", "totalSize": "715", "userId": 600004701
            }, {
                "price": "84399.1", "totalSize": "7", "userId": 300012600017611
            }, {
                "price": "84350.5", "totalSize": "3", "userId": 600015663
            }, {
                "price": "84251.1", "totalSize": "72", "userId": 300024600000559
            }, {
                "price": "84243", "totalSize": "247", "userId": 600004701
            }, {
                "price": "84000", "totalSize": "1", "userId": 300003600001333
            }, {
                "price": "83959.2", "totalSize": "80", "userId": 300024600000559
            }, {
                "price": "83918.3", "totalSize": "501", "userId": 600004701
            }, {
                "price": "83799.3", "totalSize": "451", "userId": 600004701
            }, {
                "price": "83763.6", "totalSize": "829", "userId": 600004701
            }, {
                "price": "83755.9", "totalSize": "1006", "userId": 600004701
            }, {
                "price": "83726.7", "totalSize": "1143", "userId": 600004701
            }, {
                "price": "83695.1", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "83683.7", "totalSize": "1128", "userId": 600004701
            }, {
                "price": "83681.5", "totalSize": "1096", "userId": 600004701
            }, {
                "price": "83668.6", "totalSize": "48", "userId": 300540600005621
            }, {
                "price": "83667.2", "totalSize": "88", "userId": 300024600000559
            }, {
                "price": "83661.2", "totalSize": "929", "userId": 600004701
            }, {
                "price": "83656.8", "totalSize": "1016", "userId": 600004701
            }, {
                "price": "83640.4", "totalSize": "384", "userId": 600004701
            }, {
                "price": "83638.8", "totalSize": "1160", "userId": 600004701
            }, {
                "price": "83573.9", "totalSize": "1085", "userId": 600004701
            }, {
                "price": "83505.7", "totalSize": "33", "userId": 300083600016119
            }, {
                "price": "83489", "totalSize": "5", "userId": 300076600016119
            }, {
                "price": "83400.3", "totalSize": "50", "userId": 300606600005616
            }, {
                "price": "83393.8", "totalSize": "38", "userId": 300057600016119
            }, {
                "price": "83375.3", "totalSize": "97", "userId": 300024600000559
            }, {
                "price": "83352.4", "totalSize": "7", "userId": 300066600016119
            }, {
                "price": "83250", "totalSize": "323", "userId": 600004701
            }, {
                "price": "83101", "totalSize": "2", "userId": 300615600005616
            }, {
                "price": "83091.1", "totalSize": "484", "userId": 600004701
            }, {
                "price": "83083.3", "totalSize": "108", "userId": 300024600000559
            }, {
                "price": "83035.5", "totalSize": "347", "userId": 600004701
            }, {
                "price": "83019.2", "totalSize": "578", "userId": 600004701
            }, {
                "price": "83001.9", "totalSize": "174", "userId": 600004701
            }, {
                "price": "82996.9", "totalSize": "777", "userId": 600004701
            }, {
                "price": "82904.2", "totalSize": "300", "userId": 600004701
            }, {
                "price": "82813.1", "totalSize": "501", "userId": 600004701
            }, {
                "price": "82807", "totalSize": "194", "userId": 600004701
            }, {
                "price": "82791.4", "totalSize": "119", "userId": 300024600000559
            }, {
                "price": "82757.9", "totalSize": "742", "userId": 600004701
            }, {
                "price": "82600.3", "totalSize": "1", "userId": 600016173
            }, {
                "price": "82561.7", "totalSize": "159", "userId": 600004701
            }, {
                "price": "82499.4", "totalSize": "131", "userId": 300024600000559
            }, {
                "price": "82479.8", "totalSize": "809", "userId": 600004701
            }, {
                "price": "82386.3", "totalSize": "508", "userId": 600004701
            }, {
                "price": "82333.2", "totalSize": "385", "userId": 600004701
            }, {
                "price": "82324", "totalSize": "607", "userId": 600004701
            }, {
                "price": "82288", "totalSize": "748", "userId": 600004701
            }, {
                "price": "82232", "totalSize": "1", "userId": 300039600006233
            }, {
                "price": "82207.5", "totalSize": "145", "userId": 300024600000559
            }, {
                "price": "82026.8", "totalSize": "710", "userId": 600004701
            }, {
                "price": "82001.3", "totalSize": "1084", "userId": 600004701
            }, {
                "price": "81979.4", "totalSize": "478", "userId": 600004701
            }, {
                "price": "81915.5", "totalSize": "160", "userId": 300024600000559
            }, {
                "price": "81890.2", "totalSize": "471", "userId": 600004701
            }, {
                "price": "81835.2", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "81691.8", "totalSize": "532", "userId": 600004701
            }, {
                "price": "81623.6", "totalSize": "177", "userId": 300024600000559
            }, {
                "price": "81528.798338637378749", "totalSize": "14073", "userId": 600002551
            }, {
                "price": "81360.7", "totalSize": "56", "userId": 300119600007178
            }, {
                "price": "81331.6", "totalSize": "195", "userId": 300024600000559
            }, {
                "price": "81039.7", "totalSize": "216", "userId": 300024600000559
            }, {
                "price": "81003.8", "totalSize": "47", "userId": 300012600015761
            }, {
                "price": "80890.9", "totalSize": "238", "userId": 600004701
            }, {
                "price": "80854.1", "totalSize": "57", "userId": 300009600007056
            }, {
                "price": "80747.7", "totalSize": "238", "userId": 300024600000559
            }, {
                "price": "80455.8", "totalSize": "263", "userId": 300024600000559
            }, {
                "price": "80163.8", "totalSize": "290", "userId": 300024600000559
            }, {
                "price": "80000", "totalSize": "5", "userId": 300007600000977
            }, {
                "price": "79975.3", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "79871.9", "totalSize": "321", "userId": 300024600000559
            }, {
                "price": "78378.8", "totalSize": "48", "userId": 300014600015761
            }, {
                "price": "78115.4", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "78001", "totalSize": "20", "userId": 600017575
            }, {
                "price": "76255.5", "totalSize": "1", "userId": 300003600015866
            }, {
                "price": "74395.6", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "74247.5", "totalSize": "2", "userId": 300008600010168
            }, {
                "price": "73095.1", "totalSize": "2", "userId": 300039600006233
            }, {
                "price": "72535.7", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "70675.8", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "68816", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "66956.1", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "65096.2", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "64966.5", "totalSize": "2", "userId": 300008600010168
            }, {
                "price": "63958.2", "totalSize": "2", "userId": 300039600006233
            }, {
                "price": "63236.3", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "61376.4", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "60614.8", "totalSize": "10", "userId": 300603600005616
            }, {
                "price": "59516.5", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "57656.6", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "55796.7", "totalSize": "2", "userId": 300003600015866
            }, {
                "price": "55685.6", "totalSize": "2", "userId": 300008600010168
            }, {
                "price": "55456.6", "totalSize": "17", "userId": 300004600003981
            }, {
                "price": "55321", "totalSize": "1", "userId": 300011600015725
            }, {
                "price": "55000", "totalSize": "2", "userId": 300029600000866
            }, {
                "price": "54821.3", "totalSize": "2", "userId": 300039600006233
            }, {
                "price": "50000", "totalSize": "6", "userId": 300030600000866
            }, {
                "price": "46404.7", "totalSize": "3", "userId": 300008600010168
            }, {
                "price": "37123.7", "totalSize": "4", "userId": 300008600010168
            }, {
                "price": "20000", "totalSize": "1", "userId": 300029600000866
            }, {
                "price": "14500", "totalSize": "5", "userId": 300007600006233
            }, {
                "price": "14000", "totalSize": "13", "userId": 300030600004117
            }, {
                "price": "13500", "totalSize": "5", "userId": 300007600006233
            }, {
                "price": "13000", "totalSize": "13", "userId": 300030600004117
            }, {
                "price": "12833.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "12666.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "12605.9", "totalSize": "1", "userId": 600016173
            }, {
                "price": "12500", "totalSize": "6", "userId": 300034600004117
            }, {
                "price": "12499.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "12333.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "12166.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "12000", "totalSize": "14", "userId": 300030600004117
            }, {
                "price": "11999.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "11990", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11980", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11970", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11960", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11950", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11940", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11930", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11920", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11910", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11900", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11890", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11880", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11870", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11860", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11850", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11840", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11833.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "11830", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11820", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11810", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11800", "totalSize": "207", "userId": 700001600000866
            }, {
                "price": "11790", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11780", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11770", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11760", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11750", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11740", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11730", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11720", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11710", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11700", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11690", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11680", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11670", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11666.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "11660", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11650", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11640", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11630", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11620", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11610", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11600", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11590", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11580", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11570", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11560", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11550", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11540", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11530", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11520", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11510", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11500", "totalSize": "12", "userId": 300007600006233
            }, {
                "price": "11499.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "11490", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11480", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11470", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11460", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11450", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11440", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11430", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11420", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11410", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11400", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11390", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11380", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11370", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11360", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11350", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11340", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11333.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "11330", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11320", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11310", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11300", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11290", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11280", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11270", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11260", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11250", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11240", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11230", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11220", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11210", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11200", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11190", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11180", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11170", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11166.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "11160", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11150", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11140", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11130", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11120", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11110", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11100", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11090", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11080", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11070", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11060", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11050", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11040", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11030", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11020", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "11010", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "11000", "totalSize": "21", "userId": 300007600006233
            }, {
                "price": "10999.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "10990", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10983.3", "totalSize": "10", "userId": 300002600010750
            }, {
                "price": "10980", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10976.6", "totalSize": "20", "userId": 300003600010750
            }, {
                "price": "10970", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10966.6", "totalSize": "10", "userId": 300002600010750
            }, {
                "price": "10963.3", "totalSize": "20", "userId": 300003600010750
            }, {
                "price": "10960", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10950", "totalSize": "26", "userId": 300003600010750
            }, {
                "price": "10949.9", "totalSize": "10", "userId": 300002600010750
            }, {
                "price": "10940", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10933.3", "totalSize": "10", "userId": 300002600010750
            }, {
                "price": "10930", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10920", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10916.6", "totalSize": "10", "userId": 300002600010750
            }, {
                "price": "10910", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10900", "totalSize": "18", "userId": 300002600010750
            }, {
                "price": "10890", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10880", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10870", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10860", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10850", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10840", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10833.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "10830", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10820", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10810", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10800", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10790", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10780", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10770", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10760", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10750", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10740", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10730", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10720", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10710", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10700", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10690", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10680", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10670", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10666.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "10660", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10650", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10640", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10630", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10620", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10610", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10600", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10590", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10580", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10570", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10560", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10550", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10540", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10530", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10520", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10510", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10500", "totalSize": "12", "userId": 300007600006233
            }, {
                "price": "10499.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "10490", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10480", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10470", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10460", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10450", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10440", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10430", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10420", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10410", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10400", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10390", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10380", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10370", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10360", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10350", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10340", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10333.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "10330", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10320", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10310", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10300", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10290", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10280", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10270", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10260", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10250", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10240", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10230", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10220", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10210", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10200", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10190", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10180", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10170", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10166.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "10160", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10150", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10140", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10130", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10120", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10110", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10100", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10090", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10080", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10070", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10060", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10050", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10040", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10030", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10020", "totalSize": "7", "userId": 300023600006715
            }, {
                "price": "10010", "totalSize": "6", "userId": 300023600006715
            }, {
                "price": "10000", "totalSize": "332", "userId": 600015128
            }, {
                "price": "9999.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "9999", "totalSize": "1", "userId": 600015466
            }, {
                "price": "9833.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "9666.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "9499.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "9333.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "9166.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "9000", "totalSize": "5", "userId": 600010487
            }, {
                "price": "8999.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "8833.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "8666.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "8499.9", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "8333.3", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "8166.6", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "8000", "totalSize": "2", "userId": 300038600006233
            }, {
                "price": "6500", "totalSize": "8", "userId": 300047600006233
            }, {
                "price": "5000", "totalSize": "16", "userId": 600000977
            }, {
                "price": "3000", "totalSize": "66", "userId": 600016097
            }, {
                "price": "1000", "totalSize": "30", "userId": 700003600000892
            }, {
                "price": "300", "totalSize": "1", "userId": 700010600000892
            }, {
                "price": "200", "totalSize": "1", "userId": 700009600000892
            }, {
                "price": "130", "totalSize": "120", "userId": 700013600000892
            }, {
                "price": "120", "totalSize": "100", "userId": 700012600000892
            }, {
                "price": "100", "totalSize": "370", "userId": 700005600000892
            }, {
                "price": "20", "totalSize": "2", "userId": 700001600006648
            }, {
                "price": "10", "totalSize": "10", "userId": 700001600016056
            }]
        }, "instrument": {
            "base": "btc",
            "closeSpread": 0.0002000000000000,
            "commissionRate": 0.0006000000000000,
            "config": "{\"margins\":{\"5\":0.00375,\"10\":0.0075,\"20\":0.015,\"50\":0.0375,\"100\":0.075},\"simulatedMargins\":{\"5\":0.00375,\"10\":0.0075,\"20\":0.015}}",
            "createdDate": 1548950400000,
            "defaultLeverage": 20,
            "defaultStopLossRate": 0.9900000000000000,
            "defaultStopProfitRate": 100.0000000000000000,
            "id": 1,
            "indexId": 1,
            "leverage": "[5,10,20,50,100,125,200]",
            "makerFee": 0.0004000000000000,
            "maxLeverage": 125,
            "maxPosition": 200000.0000000000000000,
            "minLeverage": 1,
            "minSize": 1,
            "name": "BTC",
            "oneLotMargin": 1.0000000000000000,
            "oneLotSize": 0.0010000000000000,
            "oneMaxPosition": 16000.0000000000000000,
            "openSpread": 0.0003000000000000,
            "pricePrecision": 1,
            "quote": "usdt",
            "selected": 0,
            "settledAt": 1765353600000,
            "settlementRate": 0.0004000000000000,
            "simulatedLeverage": "[5,10,20,50,100,125,200]",
            "sort": 1,
            "status": "online",
            "stopsurplusrate": 0.0100000000000000,
            "takerFee": 0.0006000000000000,
            "updatedDate": 1765353638000
        }, "timestamp": 1765357447750
    };

    try {
        writer.produce({
            messages: [{
                key: null, value: schemaRegistry.serialize({
                    data: JSON.stringify(message), schemaType: SCHEMA_TYPE_STRING,
                }),
            }]
        });

        log(`发送成功: ${key}`);
    } catch (error) {
        console.error(`发送失败: ${error.message}`);
    }
}

export function teardown() {
    writer.close();
}